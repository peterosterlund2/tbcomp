#include "repair.hpp"
#include "huffman.hpp"
#include "threadpool.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <cassert>
#include <iostream>


RePairComp::RePairComp(std::vector<U8>& inData, int minFreq, int maxSyms)
    : sa(inData) {
    nThreads = std::thread::hardware_concurrency();
    compress((U64)minFreq, maxSyms);
}

void
RePairComp::toBitBuf(BitBufferWriter& out) {
    const int symTableSize = symbols.size();

    // Write symbol table
    out.writeBits(symTableSize, 16);
    for (int i = 0; i < symTableSize; i++) {
        out.writeBits(symbols[i].getLeft(), 16);
        out.writeBits(symbols[i].getRight(), 16);
    }

    // Compute symbol frequencies
    std::vector<U64> freq(symTableSize);
    U64 nSyms = 0;
    SymbolArray::iterator it = sa.iterAtChunk(0);
    while (true) {
        int sym = it.getSymbol();
        assert(sym != -1);
        freq[sym]++;
        nSyms++;
        if (!it.moveToNext())
            break;
    }

    // Huffman encode symbols
    Huffman huff;
    HuffCode code;
    huff.computePrefixCode(freq, code);
    code.toBitBuf(out, true);
    out.writeU64(nSyms);
    it = sa.iterAtChunk(0);
    while (true) {
        int sym = it.getSymbol();
        assert(sym != -1);
        code.encodeSymbol(sym, out);
        if (!it.moveToNext())
            break;
    }
}

namespace RePairImpl {
    using namespace boost;
    using namespace boost::multi_index;

    struct PairCand {
        U16 p1;
        U16 p2;
        int depth;
        U64 freq;
        std::vector<U64> indices;
        U64 freqPrio() const { return (freq << 8) + 255 - depth; }
        U64 cachePrio() const { return (indices.empty() ? (1ULL<<63) : 0) + freq; }
    };
    struct Pair { };
    struct Freq { };
    struct Cache { };

    using PairCandSet = multi_index_container<
        PairCand, indexed_by<
            hashed_unique< tag<Pair>,
                composite_key<
                    PairCand,
                    member<PairCand,U16,&PairCand::p1>,
                    member<PairCand,U16,&PairCand::p2>
                >
            >,
            ordered_non_unique< tag<Freq>,
                const_mem_fun<PairCand,U64,&PairCand::freqPrio>,
                std::greater<U64>
            >,
            ordered_non_unique< tag<Cache>,
                const_mem_fun<PairCand,U64,&PairCand::cachePrio>,
                std::greater<U64>
            >
        >
    >;

    struct CompressData {
        explicit CompressData(U64 minF) : minFreq(minF) {}
        PairCandSet pairCands;
        const U64 minFreq;
        S64 cacheSize = 0; // Sum of indices.size() for all PairCands
    };

    struct DeltaFreq {
        std::vector<S64> deltaFreqAZ, deltaFreqZB;
        std::vector<S64> deltaFreqAX, deltaFreqYB;
        std::vector<std::vector<U64>> vecAZ, vecZB;

        void resize(int nSym) {
            deltaFreqAZ.resize(nSym); vecAZ.resize(nSym);
            deltaFreqZB.resize(nSym); vecZB.resize(nSym);
            deltaFreqAX.resize(nSym);
            deltaFreqYB.resize(nSym);
        }
    };
}

void
RePairComp::compress(U64 minFreq, int maxSyms) {
    using namespace RePairImpl;
    CompressData cpData(minFreq);
    PairCandSet& pairCands = cpData.pairCands;
    S64& cacheSize = cpData.cacheSize;

    const size_t maxCands = std::max(128*1024, 16*maxSyms); // Heuristic limit

    const U64 maxCache = std::max(U64(64*1024*1024), sa.size() / 512);

    // Delta frequencies when transforming AXYB -> AZB
    DeltaFreq delta;

    U64 comprSize = sa.size();
    initSymbols(cpData);

    while (!pairCands.empty()) {
        if ((int)symbols.size() >= maxSyms)
            break;
        auto it = pairCands.get<Freq>().begin();
        assert(cacheSize >= 0);
        if (it->indices.empty() && it->freq * 8 <= maxCache && it->freq * 8 <= comprSize)
            refillCache(cpData, maxCache);

        const U16 X = it->p1;
        const U16 Y = it->p2;
        const U16 Z = (U16)symbols.size();
        RePairSymbol newSym;
        newSym.setPair(X, Y);
        newSym.setLengthDepth(symbols[X].getLength() + symbols[Y].getLength(),
                              std::max(symbols[X].getDepth(), symbols[Y].getDepth())+1);
        symbols.push_back(newSym);
        const int nSym = symbols.size();

        std::cout << "XY->Z: " << X << ' ' << Y << ' ' << Z
                  << " len:" << newSym.getLength() << " depth:" << newSym.getDepth() << std::endl;

        delta.resize(nSym);

        U64 nRepl = it->indices.empty() ? replacePairs(X, Y, Z, delta) :
                                          replacePairsIdxCache(it->indices, X, Y, Z, delta);

        int nAdded = 0, nRemoved = 0;
        cacheSize -= it->indices.size();
        pairCands.get<Freq>().erase(it);
        for (int i = 0; i < nSym; i++) {
            for (int k = 0; k < 2; k++) {
                U16 p1 = (k == 0) ? i : Z;
                U16 p2 = (k == 0) ? Z : i;
                U64 f  = (k == 0) ? delta.deltaFreqAZ[i] : delta.deltaFreqZB[i];
                if (f != 0) {
                    ((k == 0) ? delta.deltaFreqAZ[i] : delta.deltaFreqZB[i]) = 0;
                    std::vector<U64>& vec = (k == 0) ? delta.vecAZ[i] : delta.vecZB[i];
                    if (f >= minFreq) {
                        int d = std::max(symbols[i].getDepth(), symbols[Z].getDepth()) + 1;
                        PairCand pc { (U16)p1, (U16)p2, d, f };
                        cacheSize += vec.size();
                        pc.indices = std::move(vec);
                        pairCands.insert(std::move(pc));
                        nAdded++;
                    }
                    vec.clear();
                }
            }
        }
        pruneCache(cpData, maxCache, comprSize);
        for (int i = 0; i < nSym; i++) {
            for (int k = 0; k < 2; k++) {
                U16 p1 = (k == 0) ? i : Y;
                U16 p2 = (k == 0) ? X : i;
                S64 d  = (k == 0) ? delta.deltaFreqAX[i] : delta.deltaFreqYB[i];
                if (d != 0) {
                    ((k == 0) ? delta.deltaFreqAX[i] : delta.deltaFreqYB[i]) = 0;
                    auto it2 = pairCands.get<Pair>().find(std::make_tuple(p1,p2));
                    if (it2 != pairCands.get<Pair>().end()) {
                        pairCands.get<Pair>().modify(it2, [d](PairCand& pc) { pc.freq += d; });
                        if (it2->freq < minFreq) {
                            cacheSize -= it2->indices.size();
                            pairCands.get<Pair>().erase(it2);
                            nRemoved++;
                        }
                    }
                }
            }
        }
        comprSize -= nRepl;
        std::cout << "repl:" << nRepl << " add:" << nAdded << " remove:" << nRemoved
                  << " cand:" << pairCands.size() << " cache:" << cacheSize
                  << " compr:" << comprSize << std::endl;

        int pruneFreq = -1;
        while (pairCands.size() > maxCands) {
            auto it2 = --pairCands.get<Freq>().end();
            cacheSize -= it2->indices.size();
            pruneFreq = (int)it2->freq;
            pairCands.get<Freq>().erase(it2);
        }
        if (pruneFreq != -1)
            std::cout << "candidate prune, freq:" << pruneFreq << std::endl;
    }
}

void
RePairComp::initSymbols(RePairImpl::CompressData& cpData) {
    using namespace RePairImpl;
    PairCandSet& pairCands = cpData.pairCands;

    const U64 minFreq = cpData.minFreq;
    const int nChunks = sa.getChunks().size();

    // Create primitive symbols
    int primitiveSyms[256];
    for (int i = 0; i < 256; i++)
        primitiveSyms[i] = 0;
    {
        ThreadPool<std::array<int,256>> pool(nThreads);
        for (int ch = 0; ch < nChunks; ch++) {
            auto task = [this,ch](int workerNo) {
                std::array<int,256> result {};
                SymbolArray::iterator it = sa.iterAtChunk(ch);
                U64 end = sa.getChunks()[ch].endUsed;
                while (it.getIndex() < end) {
                    result[it.getSymbol()] = 1;
                    it.moveToNext();
                }
                return result;
            };
            pool.addTask(task);
        }
        std::array<int,256> result;
        while (pool.getResult(result))
            for (int i = 0; i < 256; i++)
                primitiveSyms[i] |= result[i];
    }
    for (int i = 0; i < 256; i++) {
        if (primitiveSyms[i]) {
            RePairSymbol s;
            s.setPrimitive(i);
            primitiveSyms[i] = symbols.size();
            symbols.push_back(s);
            std::cout << "sym:" << primitiveSyms[i] << " val:" << i << std::endl;
        }
    }

    // Transform data[] to symbol values
    {
        ThreadPool<int> pool(nThreads);
        for (int ch = 0; ch < nChunks; ch++) {
            auto task = [this,&primitiveSyms,ch](int workerNo) {
                SymbolArray::iterator it = sa.iterAtChunk(ch);
                U64 end = sa.getChunks()[ch].endUsed;
                while (it.getIndex() < end)
                    it.putSymbol(primitiveSyms[it.getSymbol()]);
                return 0;
            };
            pool.addTask(task);
        }
        int dummy;
        while (pool.getResult(dummy))
            ;
    }

    // Create initial pair candidates
    struct FreqInfo {
        std::array<U64,256*256> freq {};
    };
    FreqInfo initialFreq;
    {
        ThreadPool<FreqInfo> pool(nThreads);
        for (int ch = 0; ch < nChunks; ch++) {
            auto task = [this,ch](int workerNo) {
                FreqInfo fi;
                SymbolArray::iterator it = sa.iterAtChunk(ch);
                U64 end = sa.getChunks()[ch].endUsed;
                int symA = it.getSymbol();
                while (it.moveToNext() && it.getIndex() <= end) {
                    int symB = it.getSymbol();
                    fi.freq[symA*256+symB]++;
                    symA = symB;
                }
                return fi;
            };
            pool.addTask(task);
        }
        FreqInfo fi;
        while (pool.getResult(fi))
            for (int i = 0; i < 256*256; i++)
                initialFreq.freq[i] += fi.freq[i];
    }
    for (int i = 0; i < 256; i++) {
        for (int j = 0; j < 256; j++) {
            U64 f = initialFreq.freq[i*256+j];
            if (f >= minFreq)
                pairCands.insert(PairCand{(U16)i,(U16)j,2,f});
        }
    }
}

void
RePairComp::pruneCache(RePairImpl::CompressData& cpData, S64 maxSize, U64 maxFreq) const {
    using namespace RePairImpl;
    PairCandSet& pairCands = cpData.pairCands;
    S64& cacheSize = cpData.cacheSize;

    while (cacheSize > maxSize) {
        auto it2 = --pairCands.get<Cache>().end();
        if (it2->indices.empty() || it2->freq >= maxFreq)
            break;
        cacheSize -= it2->indices.size();
        pairCands.get<Cache>().modify(it2, [](PairCand& pc){ pc.indices.clear();
                                                             pc.indices.shrink_to_fit(); });
    }
}

void
RePairComp::refillCache(RePairImpl::CompressData& cpData, U64 maxCache) {
    using namespace RePairImpl;
    PairCandSet& pairCands = cpData.pairCands;
    S64& cacheSize = cpData.cacheSize;

    std::unordered_map<U32, std::vector<U64>> cache;
    S64 newCacheSize = 0;
    U64 minCacheFreq = maxCache;
    for (const auto& ce : pairCands.get<Cache>()) {
        if (!ce.indices.empty())
            break;
        pruneCache(cpData, maxCache - newCacheSize - ce.freq, ce.freq);
        if (cacheSize + newCacheSize + ce.freq > maxCache)
            break;
        U32 xy = (ce.p1 << 16) | ce.p2;
        cache.insert(std::make_pair(xy, std::vector<U64>()));
        newCacheSize += ce.freq;
        minCacheFreq = std::min(minCacheFreq, ce.freq);
    }
    std::cout << "refill cache: nElem:" << cache.size() << " minFreq:" << minCacheFreq << std::endl;
    LookupTable lut(cache);
    {
        SymbolArray::iterator it = sa.iter(0);
        U64 idxX = it.getIndex(); int x = it.getSymbol(); it.moveToNext();
        U64 idxY = it.getIndex(); int y = it.getSymbol(); it.moveToNext();
        while (y != -1) {
            U32 key = (((U16)x) << 16) | (U16)y;
            std::vector<U64>* vec = lut.lookup(key);
            if (vec)
                vec->push_back(idxX);
            idxX = idxY; x = y;
            idxY = it.getIndex(); y = it.getSymbol(); it.moveToNext();
        }
    }
    for (auto& e : cache) {
        U32 xy = e.first;
        U16 x = (U16)(xy >> 16);
        U16 y = (U16)(xy & 0xffff);
        std::vector<U64>& vec = e.second;
        auto it2 = pairCands.get<Pair>().find(std::make_tuple(x,y));
        cacheSize += vec.size();
        pairCands.get<Pair>().modify(it2, [&vec](PairCand& pc) { pc.indices = std::move(vec); });
    }
    std::cout << "refill cache: nElem:" << cache.size() << " cacheSize:" << cacheSize << std::endl;
}

U64
RePairComp::replacePairs(int X, int Y, int Z, RePairImpl::DeltaFreq& delta) {
    std::vector<S64>& deltaFreqAZ = delta.deltaFreqAZ;
    std::vector<S64>& deltaFreqZB = delta.deltaFreqZB;
    std::vector<S64>& deltaFreqAX = delta.deltaFreqAX;
    std::vector<S64>& deltaFreqYB = delta.deltaFreqYB;

    const int nChunks = sa.getChunks().size();

    U64 nRepl = 0;
    for (int ch = 0; ch < nChunks; ch++) {
        SymbolArray::iterator inIt = sa.iterAtChunk(ch);
        SymbolArray::iterator outIt = sa.iter(inIt.getIndex());
        U64 end = sa.getChunks()[ch].endUsed;

        if (inIt.getSymbol() == -1) {
            inIt.moveToNext();
            U64 idx = outIt.getIndex();
            if (idx > 0 && sa.getUsedIdx(idx-1))
                outIt = sa.iter(idx+1);
        }

        SymbolArray::iterator itA(inIt);
        int a = itA.moveToPrev() ? itA.getSymbol() : -1;
        int x = inIt.getSymbol(); U64 idxX = inIt.getIndex(); inIt.moveToNext();
        int y = inIt.getSymbol(); U64 idxY = inIt.getIndex(); inIt.moveToNext();
        int b = inIt.getSymbol(); U64 idxB = inIt.getIndex(); inIt.moveToNext();

        while (idxX < end) {
            if (x == X && y == Y) { // Transform AXYB -> AZB
                if (a != -1) {
                    deltaFreqAZ[a]++;
                    deltaFreqAX[a]--;
                }
                if (b != -1) {
                    deltaFreqZB[b]++;
                    deltaFreqYB[b]--;
                }
                sa.setUsedIdx(idxY, false);
                outIt.putSymbol(Z);
                a = Z;
                x = b; idxX = idxB;
                y = inIt.getSymbol(); idxY = inIt.getIndex(); inIt.moveToNext();
                b = inIt.getSymbol(); idxB = inIt.getIndex(); inIt.moveToNext();
                nRepl++;
            } else {
                outIt.putSymbol(x);
                a = x;
                x = y; idxX = idxY;
                y = b; idxY = idxB;
                b = inIt.getSymbol(); idxB = inIt.getIndex(); inIt.moveToNext();
            }
        }
        SymbolArray::Chunk& chunk = const_cast<SymbolArray::Chunk&>(sa.getChunks()[ch]);
        chunk.endUsed = outIt.getIndex();
        if (outIt.getIndex() < sa.getChunks()[ch].end) {
            outIt.putSymbol(0);
            sa.setUsedIdx(outIt.getIndex()-1, false);
        }
    }

    return nRepl;
}

U64
RePairComp::replacePairsIdxCache(const std::vector<U64>& indices, int X, int Y, int Z,
                                 RePairImpl::DeltaFreq& delta) {
    std::vector<S64>& deltaFreqAZ = delta.deltaFreqAZ;
    std::vector<S64>& deltaFreqZB = delta.deltaFreqZB;
    std::vector<S64>& deltaFreqAX = delta.deltaFreqAX;
    std::vector<S64>& deltaFreqYB = delta.deltaFreqYB;
    std::vector<std::vector<U64>>& vecAZ = delta.vecAZ;
    std::vector<std::vector<U64>>& vecZB = delta.vecZB;

    U64 nRepl = 0;
    for (U64 idxX : indices) { // Transform AXYB -> AZB
        SymbolArray::iterator it = sa.iter(idxX);
        int x = it.getSymbol(); if (x != X) continue; it.moveToNext();
        int y = it.getSymbol(); if (y != Y) continue; U64 idxY = it.getIndex(); it.moveToNext();
        int b = it.getSymbol();
        SymbolArray::iterator itA = sa.iter(idxX);
        int a = itA.moveToPrev() ? itA.getSymbol() : -1;
        if (a != -1) {
            deltaFreqAZ[a]++; vecAZ[a].push_back(itA.getIndex());
            deltaFreqAX[a]--;
        }
        if (b != -1) {
            deltaFreqZB[b]++; vecZB[b].push_back(idxX);
            deltaFreqYB[b]--;
        }
        sa.combineSymbol(idxX, idxY, Z);
        nRepl++;
    }
    return nRepl;
}

void
RePairDeComp::deCompressAll(std::function<void(const std::vector<U8>&)> consumer) {
    BitBufferReader br(data);
    const int symTableSize = br.readBits(16);
    std::vector<RePairSymbol> symbols(symTableSize);
    for (int i = 0; i < symTableSize; i++) {
        U16 left = br.readBits(16);
        U16 right = br.readBits(16);
        symbols[i].setPair(left, right);
        U64 len = 1;
        int d = 1;
        if (!symbols[i].isPrimitive()) {
            len = symbols[left].getLength() +
                  symbols[right].getLength();
            d = std::max(symbols[left].getDepth(),
                         symbols[right].getDepth()) + 1;
        }
        symbols[i].setLengthDepth(len, d);
    }

    const int bufSize = 1024*1024;
    std::vector<U8> outData;
    outData.reserve(bufSize);

    HuffCode code;
    code.fromBitBuf(br);
    const U64 nSyms = br.readU64();
    std::vector<int> stack;
    for (U64 i = 0; i < nSyms; i++) {
        int sym = code.decodeSymbol(br);
        while (true) {
            const RePairSymbol& s = symbols[sym];
            if (s.isPrimitive()) {
                outData.push_back(s.getValue());
                if (outData.size() >= bufSize) {
                    consumer(outData);
                    outData.clear();
                }
                if (stack.empty())
                    break;
                sym = stack.back();
                stack.pop_back();
            } else {
                stack.push_back(s.getRight());
                sym = s.getLeft();
            }
        }
    }
    consumer(outData);
}

// ------------------------------------------------------------

LookupTable::LookupTable(std::unordered_map<U32,std::vector<U64>>& data) {
    int n = data.size();
    int nBits = 1;
    while (n > 0) {
        nBits++;
        n /= 2;
    }
    n = 1 << nBits;
    table.resize(n);
    mask = n - 1;

    for (auto& e : data) {
        U32 h = hashVal(e.first) & mask;
        while (!table[h].empty)
            h = (h + 1) & mask;
        table[h].key = e.first;
        table[h].empty = false;
        table[h].value = &e.second;
    }
}

std::vector<U64>*
LookupTable::lookup(U32 key) const {
    U32 h = hashVal(key) & mask;
    while (true) {
        const Entry& e = table[h];
        if (e.empty)
            break;
        if (e.key == key)
            return e.value;
        h = (h + 1) & mask;
    }
    return nullptr;
}
