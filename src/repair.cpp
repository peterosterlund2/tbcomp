#include "repair.hpp"
#include "huffman.hpp"
#include "threadpool.hpp"
#include "tbutil.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <cassert>
#include <iostream>

RePairComp::RePairComp(std::vector<U8>& inData, int minFreq, int maxSyms)
    : RePairComp(inData, minFreq, maxSyms, -1) {
}

RePairComp::RePairComp(std::vector<U8>& inData, int minFreq, int maxSyms, int chunkSize)
    : sa(inData, chunkSize) {
    std::cout << "nChunks:" << sa.getChunks().size()
              << " chunkSize:" << (sa.getChunks()[0].end - sa.getChunks()[0].beg) << std::endl;
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
                  << " len:" << newSym.getLength() << " depth:" << newSym.getDepth()
                  << " freq:" << it->freq << std::endl;

        delta.resize(nSym);

        U64 nRepl = it->indices.empty() ? replacePairs(X, Y, Z, delta) :
                                          replacePairsIdxCache(it->indices, X, Y, Z, delta);
        delta.deltaFreqAZ[Z] += delta.deltaFreqZB[Z];
        delta.deltaFreqZB[Z] = 0;

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
                while (it.getIndex() < end) {
                    sa.setByte(it.getIndex(), primitiveSyms[it.getSymbol()]);
                    it.moveToNext();
                }
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

    // Decide what to cache
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

    // Process all chunks
    std::vector<std::vector<std::pair<U32,U64>>> cacheVec(nThreads);
    LookupTable lut(cache);
    ThreadPool<int> pool(nThreads);
    const int nChunks = sa.getChunks().size();
    for (int ch = 0; ch < nChunks; ch++) {
        auto task = [this,&cacheVec,&lut,ch](int workerNo) {
            std::vector<std::pair<U32,U64>>& cache = cacheVec[workerNo];
            SymbolArray::iterator it = sa.iterAtChunk(ch);
            if (it.getSymbol() == -1)
                it.moveToNext();
            U64 end = sa.getChunks()[ch].endUsed;
            U64 idxX = it.getIndex(); int x = it.getSymbol(); it.moveToNext();
            U64 idxY = it.getIndex(); int y = it.getSymbol(); it.moveToNext();
            while (idxX < end) {
                U32 key = (((U16)x) << 16) | (U16)y;
                if (lut.lookup(key))
                    cache.push_back(std::make_pair(key, idxX));
                idxX = idxY; x = y;
                idxY = it.getIndex(); y = it.getSymbol(); it.moveToNext();
            }
            return 0;
        };
        pool.addTask(task);
    }
    int dummy;
    while (pool.getResult(dummy))
        ;
    for (int t = 0; t < nThreads; t++)
        for (const auto& p : cacheVec[t])
            lut.lookup(p.first)->push_back(p.second);
    cacheVec.clear();

    const int nKeys = cache.size();
    std::vector<U32> keys;
    keys.reserve(nKeys);
    for (auto& e : cache)
        keys.push_back(e.first);
    const int sortBatchSize = std::max(1, nKeys / nThreads / 2);
    for (int b = 0; b < nKeys; b += sortBatchSize) {
        auto task = [&lut,&keys,nKeys,sortBatchSize,b](int threadNo) {
            int end = std::min(b + sortBatchSize, nKeys);
            for (int i = b; i < end; i++) {
                std::vector<U64>& vec = *lut.lookup(keys[i]);
                std::sort(vec.begin(), vec.end());
            }
            return 0;
        };
        pool.addTask(task);
    }
    while (pool.getResult(dummy))
        ;

    // Fill in PairCand::indices
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

std::vector<bool>
RePairComp::computeSkipFirst(int X, int Y) {
    const int nChunks = sa.getChunks().size();
    std::vector<bool> skipFirst(nChunks+1, false);
    if (X == Y) {
        std::vector<U64> prevNumSame(nChunks, 0);
        std::vector<U8> prevAllSame(nChunks, false); // vector<bool> not thread safe
        ThreadPool<int> pool(nThreads);
        for (int ch = 1; ch < nChunks; ch++) {
            auto task = [this,X,&prevNumSame,&prevAllSame,ch](int threadNo) {
                SymbolArray::iterator it = sa.iterAtChunk(ch);
                if (it.getSymbol() == -1)
                    it.moveToNext();
                SymbolArray::iterator it2(it);
                int s1 = it.getSymbol(); it.moveToNext();
                int s2 = it.getSymbol();
                if (s1 == X && s2 == X) {
                    SymbolArray::iterator prevChunkIter = sa.iterAtChunk(ch-1);
                    if (prevChunkIter.getSymbol() == -1)
                        prevChunkIter.moveToNext();
                    U64 numSame = 0;
                    bool allSame = true;
                    U64 prevChunkStart = prevChunkIter.getIndex();
                    if (sa.getChunkIdx(prevChunkStart) == ch - 1) {
                        while (it2.getIndex() != prevChunkStart) {
                            if (!it2.moveToPrev())
                                assert(false);
                            if (it2.getSymbol() == X) {
                                numSame++;
                            } else {
                                allSame = false;
                                break;
                            }
                        }
                    }
                    prevNumSame[ch] = numSame;
                    prevAllSame[ch] = allSame;
                }
                return 0;
            };
            pool.addTask(task);
        }
        int dummy;
        while (pool.getResult(dummy))
            ;
        for (int ch = 1; ch < nChunks; ch++) {
            if (prevAllSame[ch])
                prevNumSame[ch] += prevNumSame[ch-1];
            skipFirst[ch] = (prevNumSame[ch] % 2) != 0;
        }
    }
    return skipFirst;
}

U64
RePairComp::replacePairs(int X, int Y, int Z, RePairImpl::DeltaFreq& delta) {
    std::vector<S64>& deltaFreqAZ = delta.deltaFreqAZ;
    std::vector<S64>& deltaFreqZB = delta.deltaFreqZB;
    std::vector<S64>& deltaFreqAX = delta.deltaFreqAX;
    std::vector<S64>& deltaFreqYB = delta.deltaFreqYB;
    const int nSym = deltaFreqAZ.size();

    std::vector<bool> skipFirst = computeSkipFirst(X, Y);

    struct Result {
        Result(int nSym)
            : nRepl(0), deltaFreqAZ(nSym), deltaFreqZB(nSym),
              deltaFreqAX(nSym), deltaFreqYB(nSym) {
        }
        U64 nRepl;
        std::vector<S64> deltaFreqAZ;
        std::vector<S64> deltaFreqZB;
        std::vector<S64> deltaFreqAX;
        std::vector<S64> deltaFreqYB;
    };

    std::mutex mutex;
    ThreadPool<Result> pool(nThreads);
    const int nChunks = sa.getChunks().size();
    for (int ch = 0; ch < nChunks; ch++) {
        auto task = [this,X,Y,Z,nSym,nChunks,&skipFirst,&mutex,ch](int threadNo) {
            Result res(nSym);

            SymbolArray::iterator inIt = sa.iterAtChunk(ch);
            SymbolArray::iterator outIt(inIt);
            U64 beg = sa.getChunks()[ch].beg;
            U64 end = sa.getChunks()[ch].endUsed;

            if (beg >= end)
                return res;

            mutex.lock();
            bool locked = true;

            if (inIt.getSymbol() == -1) {
                inIt.moveToNext();
                if (inIt.getIndex() >= end) {
                    mutex.unlock();
                    return res;
                }
                U64 idx = outIt.getIndex();
                if (idx > 0 && sa.getUsedIdx(idx-1))
                    outIt = sa.iter(idx+1);
            }

            if (skipFirst[ch]) {
                int s = inIt.getSymbol();
                inIt.moveToNext();
                outIt.putSymbol(s);
            }

            SymbolArray::iterator itA(inIt);
            int a = itA.moveToPrev() ? itA.getSymbol() : -1;
            int x = inIt.getSymbol(); U64 idxX = inIt.getIndex(); inIt.moveToNext();
            int y = inIt.getSymbol(); U64 idxY = inIt.getIndex(); inIt.moveToNext();
            int b = inIt.getSymbol(); U64 idxB = inIt.getIndex(); inIt.moveToNext();

            U64 endLock = end;
            if (ch + 1 < nChunks) {
                endLock = beg;
                SymbolArray::iterator endLockIt = sa.iterAtChunk(ch+1);
                if (endLockIt.moveToPrev()) {
                    U64 nextBegin = endLockIt.getIndex();
                    int cnt = 0;
                    while (endLockIt.moveToPrev()) {
                        if (endLockIt.getIndex()+1 < nextBegin - 64) {
                            if (++cnt >= 2) {
                                endLock = endLockIt.getIndex();
                                break;
                            }
                        }
                    }
                }
            }
            U64 lastY = 0;
            while (idxX < end) {
                if (inIt.getIndex() >= endLock) {
                    if (!locked) {
                        mutex.lock();
                        locked = true;
                    }
                } else if (outIt.getIndex() >= beg + 64) {
                    if (locked) {
                        mutex.unlock();
                        locked = false;
                    }
                }
                if (x == X && y == Y) { // Transform AXYB -> AZB
                    if (a != -1) {
                        res.deltaFreqAZ[a]++;
                        res.deltaFreqAX[a]--;
                    }
                    if (b != -1) {
                        res.deltaFreqZB[b]++;
                        res.deltaFreqYB[b]--;
                    }
                    sa.setUsedIdx(idxY, false);
                    outIt.putSymbol(Z);
                    lastY = idxY;
                    a = Z;
                    x = b; idxX = idxB;
                    y = inIt.getSymbol(); idxY = inIt.getIndex(); inIt.moveToNext();
                    b = inIt.getSymbol(); idxB = inIt.getIndex(); inIt.moveToNext();
                    res.nRepl++;
                } else {
                    outIt.putSymbol(x);
                    a = x;
                    x = y; idxX = idxY;
                    y = b; idxY = idxB;
                    b = inIt.getSymbol(); idxB = inIt.getIndex(); inIt.moveToNext();
                }
            }
            sa.setChunkEnd(ch, outIt.getIndex());
            if (outIt.getIndex() < sa.getChunks()[ch].end) {
                outIt.putSymbol(0);
                sa.setUsedIdx(outIt.getIndex()-1, false);
            }
            skipFirst[std::max(ch+1,sa.getChunkIdx(lastY))] = false;
            if (locked)
                mutex.unlock();

            return res;
        };
        pool.addTask(task);
    }
    U64 nRepl = 0;
    Result res(0);
    while (pool.getResult(res)) {
        nRepl += res.nRepl;
        for (int i = 0; i < nSym; i++) {
            deltaFreqAZ[i] += res.deltaFreqAZ[i];
            deltaFreqZB[i] += res.deltaFreqZB[i];
            deltaFreqAX[i] += res.deltaFreqAX[i];
            deltaFreqYB[i] += res.deltaFreqYB[i];
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
        SymbolArray::iterator itA(it);
        int x = it.getSymbol(); if (x != X) continue; it.moveToNext();
        int y = it.getSymbol(); if (y != Y) continue; U64 idxY = it.getIndex(); it.moveToNext();
        int b = it.getSymbol();
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
