#include "test.hpp"
#include "huffman.hpp"
#include "repair.hpp"
#include "symbolarray.hpp"
#include "tbutil.hpp"
#include "textio.hpp"
#include "posindex.hpp"
#include "threadpool.hpp"
#include <utility>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cassert>

void
Test::runTests() {
    testReadWriteBits();
    testReadWriteU64();
    testEncodeDecode();
    testFibFreq();
    testLookupTable();
    testSymArray();
    testSymArrayStraddle();
    testSymArrayEmptyChunk();
    testRePair();
    testSwapColors();
    testThreadPool();
}

void
Test::testReadWriteBits() {
    {
        BitBufferWriter bw;
        for (int i = 0; i < 64; i++) {
            U64 val = (1ULL << i) - 1;
            bw.writeBits(val, i);
            bw.writeBits(i % 2, 1);
        }

        const std::vector<U8>& buf = bw.getBuf();
        BitBufferReader br(&buf[0]);
        for (int i = 0; i < 64; i++) {
            U64 val = br.readBits(i);
            bool val2 = br.readBit();
            assert(val == (1ULL << i) - 1);
            assert(val2 == ((i % 2) != 0));
        }
    }

    {
        for (int k = 0; k < 2; k++) {
            int bits = 5;
            BitBufferWriter bw;
            for (int i = 0; i < (1<<bits); i++) {
                bw.writeBits(i, bits);
            }

            const std::vector<U8>& buf = bw.getBuf();
            BitBufferReader br(&buf[0]);
            for (int i = 0; i < (1<<bits); i++) {
                for (int j = bits-1; j >= 0; j--) {
                    bool b = k ? br.readBits(1) : br.readBit();
                    bool expected = ((1ULL << j) & i) != 0;
                    assert(b == expected);
                }
            }
        }
    }
}

void
Test::testReadWriteU64() {
    BitBufferWriter bw;
    int N = 70000;
    for (int i = 0; i < N; i++) {
        bw.writeU64(i);
    }
    U64 val = 1000000000000000000ULL;
    bw.writeU64(val);

    const std::vector<U8>& buf = bw.getBuf();
    BitBufferReader br(&buf[0]);
    for (int i = 0; i < N; i++) {
        U64 val = br.readU64();
        assert(val == (U64)i);
    }
    U64 val2 = br.readU64();
    assert(val2 == val);
}

void
Test::encodeDecode(const std::vector<int>& in) {
    const size_t N = in.size();
    const int maxVal = *std::max_element(in.begin(), in.end());
    std::vector<U64> freq(maxVal+1);
    for (int v : in)
        freq[v]++;

    BitBufferWriter bw;
    {
        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);

        code.toBitBuf(bw, true);
        bw.writeU64(N);
        huff.encode(in, code, bw);
    }
    const std::vector<U8>& buf = bw.getBuf();

    std::vector<int> out;
    {
        Huffman huff;
        HuffCode code;
        BitBufferReader br(&buf[0]);

        code.fromBitBuf(br);
        U64 len = br.readU64();
        huff.decode(br, len, code, out);
    }

    assert(out.size() == N);
    for (size_t i = 0; i < N; i++)
        assert(in[i] == out[i]);
}

void
Test::testEncodeDecode() {
    encodeDecode({13,13});
    encodeDecode({1,2,3,4,5});
    encodeDecode({0,0,0});
    encodeDecode({1,10,100,1000,10000});

    std::vector<int> data;
    for (int i = 0; i < 100; i++)
        data.push_back(i % 12);
    encodeDecode(data);
}

void
Test::testFibFreq() {
    std::vector<U64> freq;
    const int N = 64;
    U64 a = 1;
    U64 b = 1;
    for (int i = 0; i < N; i++) {
        freq.push_back(a);
        U64 c = a + b;
        a = b;
        b = c;
    }
    Huffman huff;
    HuffCode code;
    huff.computePrefixCode(freq, code);

    std::vector<int> data;
    for (int i = 0; i < N; i++)
        data.push_back(i);

    BitBufferWriter bw;
    huff.encode(data, code, bw);

    const std::vector<U8>& buf = bw.getBuf();
    BitBufferReader br(&buf[0]);

    std::vector<int> data2;
    huff.decode(br, N, code, data2);

    assert(data.size() == data2.size());
    for (int i = 0; i < N; i++)
        assert(data[i] == data2[i]);
}

void
Test::testLookupTable() {
    std::unordered_map<U32, std::vector<U64>> cache;
    cache.insert(std::make_pair(17, std::vector<U64>()));
    cache.insert(std::make_pair(132, std::vector<U64>{1,2,3}));
    LookupTable lut(cache);
    std::vector<U64>* vec = lut.lookup(18);
    assert(!vec);
    vec = lut.lookup(17);
    assert(vec);
    vec->push_back(111);
    assert(cache.find(17)->second.size() == 1);
    assert(cache.find(17)->second[0] == 111);
    vec = lut.lookup(132);
    assert(vec);
    vec->push_back(12);
    assert(cache.find(132)->second.size() == 4);
    assert(cache.find(132)->second[0] == 1);
    assert(cache.find(132)->second[1] == 2);
    assert(cache.find(132)->second[2] == 3);
    assert(cache.find(132)->second[3] == 12);
    vec = lut.lookup(1);
    assert(!vec);
}

void
Test::testSymArray() {
    std::vector<U8> data { 1,2,3,4,5,6,7,8 };
    SymbolArray sa(data, 4);
    assert(sa.size() == 8);
    for (int i = 0; i < 8; i++)
        assert(sa.getUsedIdx(i));

    SymbolArray::iterator it = sa.iter(0);
    for (int i = 0; i < 8; i++) {
        assert(it.getSymbol() == i + 1);
        assert(it.getIndex() == (U64)i);
        bool ok = it.moveToNext();
        assert(ok == (i < 7));
    }
    assert(it.getSymbol() == -1);

    it = sa.iter(7);
    for (int i = 7; i >= 0; i--) {
        assert(it.getSymbol() == i + 1);
        assert(it.getIndex() == (U64)i);
        bool ok = it.moveToPrev();
        assert(ok == (i > 0));
    }

    it = sa.iter(0);
    for (int i = 0; i < 4; i++) {
        it.putSymbol(256+i);
        assert(it.getIndex() == (U64)(i*2+2));
        assert(sa.iter(i*2).getSymbol() == 256+i);
    }

    it = sa.iter(0);
    for (int i = 0; i < 8; i++)
        it.putSymbol(i+1);
    assert(sa.getChunks().size() == 2);
    it = sa.iter(0);
    it.putSymbol(7);
    it.putSymbol(300);
    sa.setChunkEnd(0, 3);
    sa.iter(5).putSymbol(400);
    sa.setChunkUsedRange(1, 5, 7);
    it = sa.iter(5);
    assert(it.getSymbol() == 400);
    assert(it.moveToPrev());
    assert(it.getIndex() == 1);
    assert(it.getSymbol() == 300);
    assert(it.moveToPrev());
    assert(it.getIndex() == 0);
    assert(it.getSymbol() == 7);
    assert(!it.moveToPrev());

    it = sa.iter(1);
    assert(it.moveToNext());
    assert(it.getIndex() == 5);
    assert(it.getSymbol() == 400);

    // combineSymbol
    sa.setChunkUsedRange(0, 0, 4);
    sa.setChunkUsedRange(1, 4, 8);
    it = sa.iter(0);
    for (int i = 0; i < 8; i++) {
        U64 idx = it.getIndex();
        it.putSymbol(0);
        sa.setByte(idx, i+1);
    }
    sa.combineSymbol(3, 4, 17);
    assert(sa.iter(3).getSymbol() == 17);
    assert(sa.iter(4).getSymbol() == -1);
    assert(sa.iter(5).getSymbol() == 6);
    sa.combineSymbol(3, 5, 18);
    assert(sa.iter(3).getSymbol() == 18);
    assert(sa.iter(4).getSymbol() == -1);
    assert(sa.iter(5).getSymbol() == -1);
    assert(sa.iter(6).getSymbol() == 7);
    sa.combineSymbol(3, 6, 1800);
    assert(sa.iter(3).getSymbol() == 1800);
    assert(sa.iter(4).getSymbol() == -1);
    assert(sa.iter(5).getSymbol() == -1);
    assert(sa.iter(6).getSymbol() == -1);
    assert(sa.iter(7).getSymbol() == 8);
}

void
Test::testSymArrayStraddle() {
    std::vector<U8> data { 1,2,3,4,5,6,7,8 };
    SymbolArray sa(data, 4);

    SymbolArray::iterator it = sa.iter(3);
    it.putSymbol(1234);
    sa.setChunkUsedRange(1, 5, 8);
    std::vector<int> expected = { 1, 2, 3, 1234, 6, 7, 8 };
    it = sa.iter(7);
    int nSym = expected.size();
    for (int i = 0; i < nSym; i++) {
        assert(it.getSymbol() == expected[nSym-1-i]);
        bool ok = it.moveToPrev();
        assert(ok == (i < (nSym-1)));
    }

    it = sa.iter(0);
    for (int i = 0; i < nSym; i++) {
        assert(it.getSymbol() == expected[i]);
        bool ok = it.moveToNext();
        assert(ok == (i < (nSym-1)));
    }

    it = sa.iterAtChunk(1);
    assert(it.getIndex() == 5);
    assert(it.getSymbol() == 6);
}

void
Test::testSymArrayEmptyChunk() {
    std::vector<U8> data { 1,2,3,4, 5,6,7,8, 9,10,11,12 };
    SymbolArray sa(data, 4);
    sa.setChunkUsedRange(1, 0, 0);
    sa.iter(4).putSymbol(0);
    for (int i = 4; i < 8; i++)
        sa.setUsedIdx(i, false);
    SymbolArray::iterator it = sa.iterAtChunk(0);
    std::vector<int> expected = { 1,2,3,4, 9,10,11,12 };
    int nSym = expected.size();
    for (int i = 0; i < nSym; i++) {
        assert(it.getSymbol() == expected[i]);
        bool ok = it.moveToNext();
        assert(ok == (i < (nSym-1)));
    }

    sa.iter(3).putSymbol(0);
    sa.setUsedIdx(3, false);
    sa.setChunkUsedRange(0, 0, 3);
    it = sa.iter(11);
    expected = { 12,11,10,9, 3,2,1 };
    nSym = expected.size();
    for (int i = 0; i < nSym; i++) {
        assert(it.getSymbol() == expected[i]);
        bool ok = it.moveToPrev();
        assert(ok == (i < (nSym-1)));
    }
}

static std::vector<int> getSymVec(SymbolArray& sa) {
    std::vector<int> symVec;
    SymbolArray::iterator it = sa.iterAtChunk(0);
    while (true) {
        int sym = it.getSymbol();
        assert(sym != -1);
        symVec.push_back(sym);
        if (!it.moveToNext())
            break;
    }
    return symVec;
}


void
Test::testRePair() {
    {
        std::vector<U8> data(32, 0);
        RePairComp comp(data, 2, 65535, 4);
        SymbolArray& sa = comp.sa;
        std::vector<int> symVec = getSymVec(sa);
        assert(symVec.size() == 2);
    }
    {
        std::vector<U8> data(32, 0);
        RePairComp comp(data, 1, 65535, 4);
        SymbolArray& sa = comp.sa;
        std::vector<int> symVec = getSymVec(sa);
        assert(symVec.size() == 1);
    }
    {
        std::vector<U8> data(32, 0);
        RePairComp comp(data, 1, 3, 4);
        SymbolArray& sa = comp.sa;
        std::vector<int> symVec = getSymVec(sa);
        assert(symVec.size() == 8);
    }
    {
        std::vector<U8> data;
        for (int i = 0; i < 4096; i++) {
            for (int j = 0; j < 128; j++)
                data.push_back(0);
            for (int j = 0; j < 128; j++)
                data.push_back(1);
        }
        RePairComp comp(data, 1, 65535, 128);
        SymbolArray& sa = comp.sa;
        std::vector<int> symVec = getSymVec(sa);
        assert(symVec.size() == 1);
        assert(symVec[0] == 28);
    }
}

void
Test::testSwapColors() {
    {
        Position pos1;
        {
            Position posType = TextIO::readFEN("krr/8/8/8/8/8/8/KQ w");
            PosIndex pi(posType);
            pi.index2Pos(1000, pos1);
        }
        Position pos2;
        {
            Position posType = TextIO::readFEN("KRR/8/8/8/8/8/8/kq w");
            PosIndex pi(posType);
            pi.index2Pos(1000, pos2);
        }
        assert(pos1 == pos2);
    }

    {
        Position posType = TextIO::readFEN("krr/8/8/8/8/8/8/KQ w");
        PosIndex pi(posType);

        Position pos1 = posType;
        Position pos2 = TextIO::readFEN("kq/8/8/8/8/8/8/KRR b");
        U64 idx1 = pi.pos2Index(pos1);
        U64 idx2 = pi.pos2Index(pos2);
        assert(idx1 == idx2);
    }
}

void
Test::testThreadPool() {
    ThreadPool<int> pool(8);

    for (int i = 0; i < 100; i++) {
        auto task = [i](int workerNo) {
            if (i % 5 == 0)
                throw i + 1;
            return i + 1;
        };
        pool.addTask(task);
    }
    int resultSum = 0;
    int exceptionSum = 0;
    for (int i = 0; i < 100; i++) {
        try {
            int res;
            bool ok = pool.getResult(res);
            assert(ok);
            resultSum += res;
        } catch (int ex) {
            exceptionSum += ex;
        }
    }
    int res;
    bool ok = pool.getResult(res);
    assert(!ok);
    assert(exceptionSum == 970);
    assert(resultSum == 5050 - 970);
}
