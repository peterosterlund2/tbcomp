/*
 * tbcomp.cpp
 *
 *  Created on: Jul 17, 2018
 *      Author: petero
 */

#include "huffman.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h>

std::string
num2Hex(U64 num) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << num;
    return ss.str();
}

void
printBits(BitBufferReader&& buf, U64 len) {
    for (U64 i = 0; i < len; i++) {
        if (i > 0) {
            if (i % 64 == 0)
                std::cout << '\n';
            else if (i % 8 == 0)
                std::cout << ' ';
        }
        bool val = buf.readBit();
        std::cout << (val ? '1' : '0');
    }
    std::cout << std::endl;
}

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
#if 0
    {
        BitBufferWriter bw;
        for (int i = 0; i < 10; i++) {
            bw.writeU64(i);
        }
        std::cout << "numBits:" << bw.getNumBits() << std::endl;
        const std::vector<U64>& buf = bw.getBuf();
        printBits(BitBufferReader((const U8*)&buf[0]), buf.size() * 64);

        BitBufferReader br((const U8*)&buf[0]);
        for (int i = 0; i < 10; i++) {
            U64 val = br.readU64();
            std::cout << "val:" << val << std::endl;
        }
    }
#endif
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
}
