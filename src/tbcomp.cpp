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

inline std::string
num2Hex(U64 num) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << num;
    return ss.str();
}

int main(int argc, char* argv[]) {
#if 0
    {
        BitBufferWriter bw;

        for (int i = 0; i < 64; i++) {
            U64 val = (1ULL << i) - 1;
            bw.putBits(val, i);
            bw.putBits(i % 2, 1);
        }

        std::vector<U64> buf = bw.getBuf();

        BitBufferReader br((const U8*)&buf[0]);
        for (int i = 0; i < 64; i++) {
            U64 val = br.readBits(i);
            bool val2 = br.readBit();
            std::cout << "i:" << i << " val:" << num2Hex(val) << " val2:" << val2 << std::endl;
        }
    }
#endif
    {
        Huffman huff;
        HuffCode code;

        std::vector<U64> freq;
        for (int i = 1; i < argc; i++)
            freq.push_back(std::atol(argv[i]));

        huff.computePrefixCode(freq, code);
    }
}
