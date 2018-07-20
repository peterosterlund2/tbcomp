/*
 * tbcomp.cpp
 *
 *  Created on: Jul 17, 2018
 *      Author: petero
 */

#include "util.hpp"
#include "huffman.hpp"
#include "test.hpp"


int main(int argc, char* argv[]) {
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
#if 0
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
        std::vector<U64> freq(256);
        std::vector<int> data;
        while (true) {
            int c = getchar();
            if (c == EOF)
                break;
            freq[c]++;
            data.push_back(c);
        }
        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);

        BitBufferWriter bw;
        code.toBitBuf(bw, false);
        bw.writeU64(data.size());
        huff.encode(data, code, bw);

        std::cout << "numBits:" << bw.getNumBits() << std::endl;
        const std::vector<U64>& buf = bw.getBuf();
        printBits(BitBufferReader((const U8*)&buf[0]), buf.size() * 64);

        std::vector<int> data2;
        HuffCode code2;
        BitBufferReader br((const U8*)&buf[0]);
        code2.fromBitBuf(br, 256);
        U64 len = br.readU64();
        huff.decode(br, len, code, data2);
        for (int d : data2)
            std::cout << (char)d;
    }
#endif
    Test().runTests();
}
