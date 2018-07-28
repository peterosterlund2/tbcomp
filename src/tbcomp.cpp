/*
 * tbcomp.cpp
 *
 *  Created on: Jul 17, 2018
 *      Author: petero
 */

#include "util.hpp"
#include "huffman.hpp"
#include "repair.hpp"
#include "test.hpp"
#include <cstdlib>
#include <fstream>


static void usage() {
    std::cerr << "Usage: tbcomp cmd params\n";
    std::cerr << "cmd is one of:\n";
    std::cerr << " test : Run automatic tests\n";
    std::cerr << " freq : Huffman code from frequencies \n";
    std::cerr << " freqdata f1 ... fn : d1 ... dn : Frequencies and data\n";
    std::cerr << " fromfile : Frequencies and data from file\n";

    std::cerr << " huffcomp infile outfile : Huffman compress\n";
    std::cerr << " huffdecomp infile outfile : Huffman decompress\n";

    std::cerr << " repaircomp infile outfile [minFreq [maxSyms]]: Re-pair compress\n";
    std::cerr << " repairdecomp infile outfile : Re-pair decompress\n";

    std::cerr << std::flush;
    ::exit(2);
}

int main(int argc, char* argv[]) {
    if (argc < 2)
        usage();

    std::string cmd(argv[1]);

    if (cmd == "test") {
        Test().runTests();

    } else if (cmd == "freq") {
        Huffman huff;
        HuffCode code;
        std::vector<U64> freq;
        for (int i = 2; i < argc; i++)
            freq.push_back(std::atol(argv[i]));
        huff.computePrefixCode(freq, code);

    } else if (cmd == "freqdata") {
        std::vector<U64> freq;
        std::vector<int> data;
        bool sepFound = false;
        for (int i = 2; i < argc; i++) {
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
        const std::vector<U8>& buf = bw.getBuf();
        printBits(BitBufferReader(&buf[0]), buf.size() * 8);

        BitBufferReader br(&buf[0]);
        std::vector<int> data2;
        huff.decode(br, data.size(), code, data2);
        std::cout << "data2: " << data2 << std::endl;

    } else if (cmd == "fromfile") {
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
        const std::vector<U8>& buf = bw.getBuf();
        printBits(BitBufferReader(&buf[0]), buf.size() * 8);

        std::vector<int> data2;
        HuffCode code2;
        BitBufferReader br(&buf[0]);
        code2.fromBitBuf(br, 256);
        U64 len = br.readU64();
        huff.decode(br, len, code, data2);
        for (int d : data2)
            std::cout << (char)d;

    } else if (cmd == "huffcomp") {
        if (argc != 4)
            usage();
        std::ifstream inF(argv[2]);
        std::ofstream outF(argv[3]);

        std::cout << "Reading..." << std::endl;
        std::vector<U64> freq(256);
        std::vector<int> data;
        char c;
        while (inF.get(c)) {
            freq[(U8)c]++;
            data.push_back((U8)c);
        }

        std::cout << "Computing prefix code..." << std::endl;
        Huffman huff;
        HuffCode code;
        huff.computePrefixCode(freq, code);

        std::cout << "Encoding..." << std::endl;
        BitBufferWriter bw;
        code.toBitBuf(bw, false);
        bw.writeU64(data.size());
        huff.encode(data, code, bw);

        std::cout << "Writing..." << std::endl;
        const std::vector<U8>& buf = bw.getBuf();
        outF.write((const char*)&buf[0], buf.size());

    } else if (cmd == "huffdecomp") {
        if (argc != 4)
            usage();
        std::ifstream inF(argv[2]);
        std::ofstream outF(argv[3]);

        std::cout << "Reading..." << std::endl;
        std::vector<U8> inData;
        char c;
        while (inF.get(c))
            inData.push_back((U8)c);

        std::cout << "Decoding..." << std::endl;
        BitBufferReader br(&inData[0]);
        std::vector<int> data;
        Huffman huff;
        HuffCode code;
        code.fromBitBuf(br, 256);
        U64 len = br.readU64();
        huff.decode(br, len, code, data);

        std::cout << "Writing..." << std::endl;
        std::vector<char> cVec;
        cVec.reserve(data.size());
        for (int d : data)
            cVec.push_back((char)d);
        outF.write(&cVec[0], cVec.size());

    } else if (cmd == "repaircomp") {
        if (argc < 4 || argc > 6)
            usage();
        std::ifstream inF(argv[2]);
        std::ofstream outF(argv[3]);
        int minFreq = 8;
        if (argc > 4)
            minFreq = std::atoi(argv[4]);
        if (minFreq < 1)
            usage();
        int maxSyms = 65535;
        if (argc > 5)
            maxSyms = std::atoi(argv[5]);
        if (maxSyms < 256 || maxSyms > 65535)
            usage();

        std::cout << "Reading..." << std::endl;
        std::vector<U8> data;
        char c;
        while (inF.get(c))
            data.push_back((U8)c);

        std::cout << "Compressing..." << std::endl;
        RePairComp comp(data, minFreq, maxSyms);

        std::cout << "Encoding..." << std::endl;
        BitBufferWriter bw;
        comp.toBitBuf(bw);

        std::cout << "Writing..." << std::endl;
        const std::vector<U8>& buf = bw.getBuf();
        outF.write((const char*)&buf[0], buf.size());

    } else if (cmd == "repairdecomp") {
        if (argc != 4)
            usage();
        std::ifstream inF(argv[2]);
        std::ofstream outF(argv[3]);

        std::cout << "Reading..." << std::endl;
        std::vector<U8> inData;
        char c;
        while (inF.get(c))
            inData.push_back((U8)c);

        std::cout << "Decoding..." << std::endl;
        RePairDeComp deComp(&inData[0]);
        std::vector<U8> outData;
        deComp.deCompressAll(outData);

        std::cout << "Writing..." << std::endl;
        outF.write((const char*)&outData[0], outData.size());

    } else {
        usage();
    }
}
