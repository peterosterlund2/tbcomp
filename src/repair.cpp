#include "repair.hpp"
#include "huffman.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <unordered_map>
#include <cassert>
#include <iostream>


RePairComp::RePairComp(std::vector<U8>& inData, int minFreq, int maxSyms)
    : data(inData) {
    usedIdx.assign((inData.size() + 63) / 64, ~(0ULL));
    compress((U64)minFreq, maxSyms);
}

void RePairComp::toBitBuf(BitBufferWriter& out) {
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
    U64 i = 0;
    while (i < data.size()) {
        int sym = getData(i);
        assert(sym != -1);
        freq[sym]++;
        nSyms++;
        i += symbols[sym].getLength();
    }

    // Huffman encode symbols
    Huffman huff;
    HuffCode code;
    huff.computePrefixCode(freq, code);
    code.toBitBuf(out, true);
    out.writeU64(nSyms);
    i = 0;
    while (i < data.size()) {
        int sym = getData(i);
        assert(sym != -1);
        code.encodeSymbol(sym, out);
        i += symbols[sym].getLength();
    }
}

void
RePairComp::compress(U64 minFreq, int maxSyms) {
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
    PairCandSet pairCands;
    const size_t maxCands = std::max(128*1024, 16*maxSyms); // Heuristic limit

    const U64 maxCache = std::max(U64(32*1024*1024), data.size() / 1024);
    S64 cacheSize = 0; // Sum of indices.size() for all PairCand
    // Delta frequencies when transforming AXYB -> AZB
    std::vector<S64> deltaFreqAZ, deltaFreqZB;
    std::vector<S64> deltaFreqAX, deltaFreqYB;
    std::vector<std::vector<U64>> vecAZ, vecZB;

    const U64 dataLen = data.size();
    U64 comprSize = dataLen;
    {
        int primitiveSyms[256];
        for (int i = 0; i < 256; i++)
            primitiveSyms[i] = 0;
        for (U64 i = 0; i < dataLen; i++)
            primitiveSyms[data[i]] = 1;
        for (int i = 0; i < 256; i++) {
            if (primitiveSyms[i]) {
                RePairSymbol s;
                s.setPrimitive(i);
                primitiveSyms[i] = symbols.size();
                symbols.push_back(s);
                std::cout << "sym:" << primitiveSyms[i] << " val:" << i << std::endl;
            }
        }
        for (U64 i = 0; i < dataLen; i++)
            data[i] = primitiveSyms[data[i]];
    }
    {
        U64 initialFreq[256][256];
        for (int i = 0; i < 256; i++)
            for (int j = 0; j < 256; j++)
                initialFreq[i][j] = 0;
        for (U64 i = 1; i < dataLen; i++)
            initialFreq[data[i-1]][data[i]]++;
        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 256; j++) {
                U64 f = initialFreq[i][j];
                if (f >= minFreq)
                    pairCands.insert(PairCand{(U16)i,(U16)j,2,f});
            }
        }
    }

    while (!pairCands.empty()) {
        if ((int)symbols.size() >= maxSyms)
            break;
        auto it = pairCands.get<Freq>().begin();
        assert(cacheSize >= 0);
        if (it->indices.empty() && it->freq * 8 <= maxCache) { // Refill cache
            std::unordered_map<U32, std::vector<U64>> cache;
            S64 newCacheSize = 0;
            U64 minCacheFreq = maxCache;
            for (const auto& ce : pairCands.get<Cache>()) {
                if (!ce.indices.empty())
                    break;
                while (cacheSize + newCacheSize + ce.freq > maxCache) {
                    auto it2 = pairCands.get<Cache>().end(); --it2;
                    if (it2->indices.empty() || it2->freq >= ce.freq)
                        break;
                    cacheSize -= it2->indices.size();
                    pairCands.get<Cache>().modify(it2, [](PairCand& pc){ pc.indices.clear();
                                                                         pc.indices.shrink_to_fit(); });
                }
                if (cacheSize + newCacheSize + ce.freq > maxCache)
                    break;
                U32 xy = (ce.p1 << 16) | ce.p2;
                cache.insert(std::make_pair(xy, std::vector<U64>()));
                newCacheSize += ce.freq;
                minCacheFreq = std::min(minCacheFreq, ce.freq);
            }
            std::cout << "refill cache: nElem:" << cache.size() << " minFreq:" << minCacheFreq << std::endl;
            {
                U64 idx = 0;
                U64 idxX = idx; int x = getNextSymbol(idx);
                U64 idxY = idx; int y = getNextSymbol(idx);
                while (y != -1) {
                    U32 key = (((U16)x) << 16) | (U16)y;
                    auto it = cache.find(key);
                    if (it != cache.end())
                        it->second.push_back(idxX);
                    idxX = idxY; x = y;
                    idxY = idx;  y = getNextSymbol(idx);
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

        deltaFreqAZ.resize(nSym); vecAZ.resize(nSym);
        deltaFreqZB.resize(nSym); vecZB.resize(nSym);
        deltaFreqAX.resize(nSym);
        deltaFreqYB.resize(nSym);

        U64 nRepl = 0;
        if (it->indices.empty()) {
            U64 idx = 0;
            int a = -1;
            U64 idxX = idx; int x = getNextSymbol(idx);
            U64 idxY = idx; int y = getNextSymbol(idx);
            U64 idxB = idx; int b = getNextSymbol(idx);
            while (y != -1) {
                if (x == X && y == Y) { // Transform AXYB -> AZB
                    if (a != -1) {
                        deltaFreqAZ[a]++;
                        deltaFreqAX[a]--;
                    }
                    if (b != -1) {
                        deltaFreqZB[b]++;
                        deltaFreqYB[b]--;
                    }
                    data[idxX] = (U8)(Z & 0xff);
                    data[idxX+1] = (U8)(Z >> 8);
                    setUsedIdx(idxY, false);

                                 a = Z;
                    idxX = idxB; x = b;
                    idxY = idx;  y = getNextSymbol(idx);
                    idxB = idx;  b = getNextSymbol(idx);
                    nRepl++;
                } else {
                                 a = x;
                    idxX = idxY; x = y;
                    idxY = idxB; y = b;
                    idxB = idx;  b = getNextSymbol(idx);
                }
            }
        } else {
            for (U64 idxX : it->indices) {
                U64 idx = idxX; int x = getNextSymbol(idx); if (x != X) continue;
                U64 idxY = idx; int y = getNextSymbol(idx); if (y != Y) continue;
                int b = getNextSymbol(idx);
                U64 idxA = idxX - 1; int a = -1;
                if (idxX > 0)
                    while (true)
                        if (((a = getData(idxA)) != -1) || (idxA-- == 0))
                            break;
                if (a != -1) {
                    deltaFreqAZ[a]++; vecAZ[a].push_back(idxA);
                    deltaFreqAX[a]--;
                }
                if (b != -1) {
                    deltaFreqZB[b]++; vecZB[b].push_back(idxX);
                    deltaFreqYB[b]--;
                }
                data[idxX] = (U8)(Z & 0xff);
                data[idxX+1] = (U8)(Z >> 8);
                setUsedIdx(idxY, false);
                nRepl++;
            }
        }

        int nAdded = 0, nRemoved = 0;
        cacheSize -= it->indices.size();
        pairCands.get<Freq>().erase(it);
        for (int i = 0; i < nSym; i++) {
            for (int k = 0; k < 2; k++) {
                U16 p1 = (k == 0) ? i : Z;
                U16 p2 = (k == 0) ? Z : i;
                U64 f  = (k == 0) ? deltaFreqAZ[i] : deltaFreqZB[i];
                if (f != 0) {
                    ((k == 0) ? deltaFreqAZ[i] : deltaFreqZB[i]) = 0;
                    std::vector<U64>& vec = (k == 0) ? vecAZ[i] : vecZB[i];
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
        for (int i = 0; i < nSym; i++) {
            for (int k = 0; k < 2; k++) {
                U16 p1 = (k == 0) ? i : Y;
                U16 p2 = (k == 0) ? X : i;
                S64 d  = (k == 0) ? -deltaFreqAX[i] : -deltaFreqYB[i];
                if (d != 0) {
                    ((k == 0) ? deltaFreqAX[i] : deltaFreqYB[i]) = 0;
                    auto it2 = pairCands.get<Pair>().find(std::make_tuple(p1,p2));
                    if (it2 != pairCands.get<Pair>().end()) {
                        pairCands.get<Pair>().modify(it2, [d](PairCand& pc) { pc.freq -= d; });
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
        std::cout << "nRepl:" << nRepl << " nAdded:" << nAdded << " nRemoved:" << nRemoved
                  << " nCand:" << pairCands.size() << " cache:" << cacheSize
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


void RePairDeComp::deCompressAll(std::vector<U8>& outData) {
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
}
