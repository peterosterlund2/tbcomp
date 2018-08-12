#include "test.hpp"
#include "huffman.hpp"
#include "repair.hpp"
#include "tbutil.hpp"
#include "textio.hpp"
#include "posindex.hpp"
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
    testSwapColors();
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
