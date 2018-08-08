/*
 * tbcomp.cpp
 *
 *  Created on: Jul 17, 2018
 *      Author: petero
 */

#include "tbutil.hpp"
#include "huffman.hpp"
#include "repair.hpp"
#include "test.hpp"
#include "position.hpp"
#include "moveGen.hpp"
#include "chessParseError.hpp"
#include "computerPlayer.hpp"
#include "syzygy/rtb-probe.hpp"
#include "posindex.hpp"
#include "parameters.hpp"
#include "textio.hpp"
#include <cstdlib>
#include <fstream>

static void idx2Pos(int argc, char* argv[]);
static void idxTest(const std::string& fen);
static void wdlDump(const std::string& tbType);

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

    std::cerr << " idx2pos nwq nwr nwb nwn nwp  nbq nbr nbb nbn nbp  idx\n";
    std::cerr << " idxtest fen\n";

    std::cerr << " wdldump tbType\n";

    std::cerr << std::flush;
    ::exit(2);
}

static void
readFile(std::ifstream& inF, std::vector<U8>& data) {
    inF.seekg(0, std::ios::end);
    U64 len = inF.tellg();
    inF.seekg(0, std::ios::beg);
    data.resize(len);
    inF.read((char*)&data[0], len);
}

int main(int argc, char* argv[]) {
    if (argc < 2)
        usage();

    try {
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
            readFile(inF, inData);

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
            readFile(inF, data);

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
            readFile(inF, inData);

            std::cout << "Decoding..." << std::endl;
            RePairDeComp deComp(&inData[0]);
            std::vector<U8> outData;
            deComp.deCompressAll(outData);

            std::cout << "Writing..." << std::endl;
            outF.write((const char*)&outData[0], outData.size());

        } else if (cmd == "idx2pos") {
            if (argc < 12 || argc > 13)
                usage();
            idx2Pos(argc, argv);

        } else if (cmd == "idxtest") {
            if (argc != 3)
                usage();
            std::string fen = argv[2];
            idxTest(fen);

        } else if (cmd == "wdldump") {
            if (argc != 3)
                usage();
            wdlDump(argv[2]);

        } else {
            usage();
        }
    } catch (std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}

static void idx2Pos(int argc, char* argv[]) {
    Position pos;
    pos.setPiece(H6, Piece::WKING);
    pos.setPiece(D8, Piece::BKING);

    int idx = 0;
    int squares[] = { A2, B2, C2, A3, B3, C3, A4, B4, C4 };
    auto placePieces = [&pos,&idx,&squares](int num, int type) {
        for (int i = 0; i < num; i++)
            pos.setPiece(squares[idx++], type);
    };

    placePieces(std::atoi(argv[2]), Piece::WQUEEN);
    placePieces(std::atoi(argv[3]), Piece::WROOK);
    placePieces(std::atoi(argv[4]), Piece::WBISHOP);
    placePieces(std::atoi(argv[5]), Piece::WKNIGHT);
    placePieces(std::atoi(argv[6]), Piece::WPAWN);

    placePieces(std::atoi(argv[7]), Piece::BQUEEN);
    placePieces(std::atoi(argv[8]), Piece::BROOK);
    placePieces(std::atoi(argv[9]), Piece::BBISHOP);
    placePieces(std::atoi(argv[10]), Piece::BKNIGHT);
    placePieces(std::atoi(argv[11]), Piece::BPAWN);

    PosIndex posIdx(pos);
    std::cout << "size:" << posIdx.tbSize() << std::endl;

    if (argc > 12) {
        U64 idx = std::atoll(argv[12]);
        Position pos;
        bool ret = posIdx.index2Pos(idx, pos);
        if (ret) {
            std::cout << TextIO::asciiBoard(pos) << TextIO::toFEN(pos) << std::endl;
        } else {
            std::cout << "Invalid position" << std::endl;
        }
    }
}

static void idxTest(const std::string& fen) {
    Position pos = TextIO::readFEN(fen);
    std::cout << TextIO::asciiBoard(pos);
    PosIndex posIdx(pos);
    U64 idx = posIdx.pos2Index(pos);
    std::cout << "idx: " << idx << " size:" << posIdx.tbSize() << std::endl;
    std::cout << TextIO::asciiBoard(pos);

    Position pos2;
    bool ret = posIdx.index2Pos(idx, pos2);
    std::cout << "ret:" << (ret?1:0) << "\n" << TextIO::asciiBoard(pos2);
}


static void
getPieces(const std::string& tbType, std::vector<int>& pieces) {
    if (tbType.length() < 1 || tbType[0] != 'k')
        throw ChessParseError("Invalid tbType: " + tbType);
    bool white = true;
    for (size_t i = 1; i < tbType.length(); i++) {
        switch (tbType[i]) {
            case 'k':
                if (!white)
                    throw ChessParseError("Invalid tbType: " + tbType);
                white = false;
                break;
            case 'q':
                pieces.push_back(white ? Piece::WQUEEN: Piece::BQUEEN);
                break;
            case 'r':
                pieces.push_back(white ? Piece::WROOK: Piece::BROOK);
                break;
            case 'b':
                pieces.push_back(white ? Piece::WBISHOP: Piece::BBISHOP);
                break;
            case 'n':
                pieces.push_back(white ? Piece::WKNIGHT: Piece::BKNIGHT);
                break;
            case 'p':
                pieces.push_back(white ? Piece::WPAWN: Piece::BPAWN);
                break;
            default:
                throw ChessParseError("Invalid tbType: " + tbType);
        }
    }
    if (white)
        throw ChessParseError("Invalid tbType: " + tbType);
}

static void setupTB() {
    UciParams::gtbPath->set("/home/petero/chess/gtb");
    UciParams::gtbCache->set("2047");
    UciParams::rtbPath->set("/home/petero/chess/rtb/wdl:"
                            "/home/petero/chess/rtb/dtz:"
                            "/home/petero/chess/rtb/6wdl:"
                            "/home/petero/chess/rtb/6dtz");
}

static void wdlDump(const std::string& tbType) {
    ComputerPlayer::initEngine();
    setupTB();

    Position pos;
    pos.setPiece(H6, Piece::WKING);
    pos.setPiece(D8, Piece::BKING);

    int squares[] = { A2, B2, C2, A3, B3, C3, A4, B4, C4 };
    std::vector<int> pieces;
    getPieces(tbType, pieces);
    if (pieces.size() > 9)
        throw ChessParseError("Too many pieces");

    for (int i = 0; i < (int)pieces.size(); i++)
        pos.setPiece(squares[i], pieces[i]);

    PosIndex posIdx(pos);
    U64 size = posIdx.tbSize();
    std::cout << "size:" << size << std::endl;

    std::vector<U8> data(size);
    std::array<U64,6> cnt{};
    for (U64 idx = 0; idx < size; idx++) {
        Position pos;
        bool ret = posIdx.index2Pos(idx, pos);
        if (ret && MoveGen::canTakeKing(pos))
            ret = false;
        int val = 3;
        if (ret) {
            int success;
            int wdl = Syzygy::probe_wdl(pos, &success);
            if (!success)
                throw ChessParseError("RTB probe failed, pos:" + TextIO::toFEN(pos));
            if (!pos.isWhiteMove())
                wdl = -wdl;
            val = wdl;
        }
        data[idx] = (U8)val;
        cnt[val+2]++;
    }

    int mostFreq = 0;
    for (int i = 0; i < 5; i++) {
        std::cout << cnt[i] << ' ';
        if (cnt[i] > cnt[mostFreq])
            mostFreq = i;
    }
    std::cout << cnt[5] << std::endl;

    for (U64 idx = 0; idx < size; idx++)
        if (data[idx] == 3)
            data[idx] = (U8)(mostFreq-2);

    std::ofstream outF("out.bin");
    outF.write((const char*)data.data(), data.size());
}

