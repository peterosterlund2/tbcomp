#include "repair.hpp"
#include "huffman.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <cassert>
#include <iostream>


RePairComp::RePairComp(std::vector<U8>& inData, int minFreq)
    : data(inData) {
    usedIdx.assign((inData.size() + 63) / 64, ~(0ULL));
    compress((U64)minFreq);
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
RePairComp::compress(U64 minFreq) {
    using namespace boost;
    using namespace boost::multi_index;

    static const U64 bigNum = 0x8000000000000000ULL;

    struct PairCand {
        U16 p1;
        U16 p2;
        int depth;
        U64 freq;
        std::vector<U64> indices;
        U64 freqPrio() const { return (freq << 8) + 255 - depth; }
        U64 cachePrio() const { return (indices.empty() ? bigNum : 0) + freq; }
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
                const_mem_fun<PairCand,U64,&PairCand::cachePrio>
            >
        >
    >;
    PairCandSet pairCands;

    const U64 maxCache = std::max(U64(32*1024*1024), data.size() / 1024);
//    U64 cacheSize = 0; // Sum of indices.size() for all PairCand
    // Delta frequencies when transforming AXYB -> AZB
    std::vector<S64> deltaFreqAZ, deltaFreqZB;
    std::vector<S64> deltaFreqAX, deltaFreqYB;

    int primitiveSyms[256];
    for (int i = 0; i < 256; i++)
        primitiveSyms[i] = 0;
    const U64 dataLen = data.size();
    U64 comprSize = dataLen;
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

    while (!pairCands.empty()) {
        if (symbols.size() >= 65535)
            break;
        auto it = pairCands.get<Freq>().begin();
        if (it->indices.empty() && it->freq * 4 <= maxCache) {
            // Refill cache
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

        deltaFreqAZ.resize(nSym);
        deltaFreqZB.resize(nSym);
        deltaFreqAX.resize(nSym);
        deltaFreqYB.resize(nSym);

        int nRepl = 0;
        if (it->indices.empty()) {
            auto getNextSymbol = [this,dataLen](U64& idx) -> int {
                if (idx >= dataLen)
                    return -1;
                int ret = getData(idx);
                idx += symbols[ret].getLength();
                return ret;
            };
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
            // Use indices
        }

        int nAdded = 0, nRemoved = 0;
        pairCands.get<Freq>().erase(it);
        for (int i = 0; i < nSym; i++) {
            for (int k = 0; k < 2; k++) {
                U16 p1 = (k == 0) ? i : Z;
                U16 p2 = (k == 0) ? Z : i;
                U64 f  = (k == 0) ? deltaFreqAZ[i] : deltaFreqZB[i];
                if (f != 0) {
                    ((k == 0) ? deltaFreqAZ[i] : deltaFreqZB[i]) = 0;
                    if (f >= minFreq) {
                        int d = std::max(symbols[i].getDepth(), symbols[Z].getDepth()) + 1;
                        pairCands.insert(PairCand{(U16)p1,(U16)p2,d,f});
                        nAdded++;
                    }
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
                    auto it = pairCands.get<Pair>().find(std::make_tuple(p1,p2));
                    if (it != pairCands.get<Pair>().end()) {
                        pairCands.get<Pair>().modify(it, [d](PairCand& pc) { pc.freq -= d; });
                        if (it->freq < minFreq) {
                            pairCands.get<Pair>().erase(it);
                            nRemoved++;
                        }
                    }
                }
            }
        }
        comprSize -= nRepl;
        std::cout << "nRepl:" << nRepl << " nAdded:" << nAdded << " nRemoved:" << nRemoved
                  << " nCand:" << pairCands.size() << " compr:" << comprSize << std::endl;

        while (pairCands.size() > 32*1024*1024)
            pairCands.get<Freq>().erase(--pairCands.get<Freq>().end());
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
