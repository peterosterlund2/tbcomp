/*
 * tbcomp.cpp
 *
 *  Created on: Jul 17, 2018
 *      Author: petero
 */

#include "util.hpp"
#include "huffman.hpp"


int main(int argc, char* argv[]) {
#if 0
    {
        BitBufferWriter bw;

        for (int i = 0; i < 64; i++) {
            U64 val = (1ULL << i) - 1;
            bw.writeBits(val, i);
            bw.writeBits(i % 2, 1);
        }

        const std::vector<U64>& buf = bw.getBuf();

        BitBufferReader br((const U8*)&buf[0]);
        for (int i = 0; i < 64; i++) {
            U64 val = br.readBits(i);
            bool val2 = br.readBit();
            std::cout << "i:" << i << " val:" << num2Hex(val) << " val2:" << val2 << std::endl;
        }
    }
#endif
#if 0
    {
        Huffman huff;
        HuffCode code;

        std::vector<U64> freq;
        for (int i = 1; i < argc; i++)
            freq.push_back(std::atol(argv[i]));

        huff.computePrefixCode(freq, code);
    }
#endif
#if 1
    {
        std::vector<U64> freq;
        std::vector<int> data;
        bool sepFound = false;
        for (int i = 1; i < argc; i++) {
            if (argv[i] == std::string(":")) {
                sepFound = true;
                continue;
            }
            if (sepFound) {
                data.push_back(std::atol(argv[i]));
            } else {
                freq.push_back(std::atol(argv[i]));
            }
        }

        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);
        BitBufferWriter bw;
        huff.encode(data, code, bw);

        std::cout << "numBits:" << bw.getNumBits() << std::endl;
        const std::vector<U64>& buf = bw.getBuf();
        printBits(BitBufferReader((const U8*)&buf[0]), buf.size() * 64);

        BitBufferReader br((const U8*)&buf[0]);
        std::vector<int> data2;
        huff.decode(br, data.size(), code, data2);
        std::cout << "data2: " << data2 << std::endl;
    }
#endif
#if 0
    {
        BitBufferWriter bw;
        int N = 20;
        for (int i = 0; i < N; i++) {
            bw.writeU64(i);
        }
        std::cout << "numBits:" << bw.getNumBits() << std::endl;
        const std::vector<U64>& buf = bw.getBuf();
        printBits(BitBufferReader((const U8*)&buf[0]), buf.size() * 64);

        BitBufferReader br((const U8*)&buf[0]);
        for (int i = 0; i < N; i++) {
            U64 val = br.readU64();
            std::cout << "val:" << val << std::endl;
        }
    }
#endif
#if 0
    {
        std::vector<U64> freq(256);
        while (true) {
            int c = getchar();
            if (c == EOF)
                break;
            freq[c]++;
        }
        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);
    }
#endif
#if 0
    {
        std::vector<U64> freq;
        U64 a = 1;
        U64 b = 1;
        for (int i = 0; i < 64; i++) {
            freq.push_back(a);
            U64 c = a + b;
            a = b;
            b = c;
        }
        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);
    }
#endif
}
