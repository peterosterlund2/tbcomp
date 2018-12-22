#include "wdlcomp.hpp"
#include "bitarray.hpp"
#include "chessParseError.hpp"
#include "syzygy/rtb-probe.hpp"
#include "parameters.hpp"
#include "computerPlayer.hpp"
#include "moveGen.hpp"
#include "textio.hpp"
#include "threadpool.hpp"

#include <fstream>


static void
setupTB() {
    UciParams::gtbPath->set("/home/petero/chess/gtb");
    UciParams::gtbCache->set("2047");
    UciParams::rtbPath->set("/home/petero/chess/rtb/wdl:"
                            "/home/petero/chess/rtb/dtz:"
                            "/home/petero/chess/rtb/6wdl:"
                            "/home/petero/chess/rtb/6dtz:"
                            "/home/petero/chess/rtb/7wdl:"
                            "/home/petero/chess/rtb/7dtz");
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

WdlCompress::WdlCompress(const std::string& tbType, bool useGini,
                         double mergeThreshold, int samplingLogFactor)
    : useGini(useGini), mergeThreshold(mergeThreshold),
      samplingLogFactor(samplingLogFactor) {
    nThreads = std::thread::hardware_concurrency();
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

    posIndex = make_unique<PosIndex>(pos);
    std::cout << "size:" << posIndex->tbSize() << std::endl;
}

void
WdlCompress::wdlDump(const std::string& outFile, int maxTreeDepth, int maxCollectorNodes) {
    PosIndex& posIdx = *posIndex;
    const U64 size = posIdx.tbSize();
    std::vector<WDLInfo> data(size);

    initializeData(data);
    computeOptimalCaptures(data);

    std::array<U64,8> cnt{};
    computeStatistics(data, cnt);
    BitArray active(data.size(), true);
    replaceDontCares(data, active);

    WDLNodeFactory factory(useGini, mergeThreshold);
    WDLUncompressedData uncompData(data);
    DecisionTree dt(factory, posIdx, uncompData, active, samplingLogFactor);
    dt.computeTree(maxTreeDepth, maxCollectorNodes, nThreads);

    writeFile(data, outFile);
}

void
WdlCompress::initializeData(std::vector<WDLInfo>& data) {
    PosIndex& posIdx = *posIndex;
    const U64 size = posIdx.tbSize();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<std::pair<int,int>> pool(nThreads);
    int nTasks = 0;
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [&posIdx,&data,size,batchSize,b](int workerNo) {
            int bestWtm = -2;
            int bestBtm = 2;
            U64 end = std::min(b + batchSize, size);
            Position pos;
            for (U64 idx = b; idx < end; idx++) {
                bool valid = posIdx.index2Pos(idx, pos);
                if (valid && MoveGen::canTakeKing(pos))
                    valid = false;
                int wdl;
                if (!valid) {
                    wdl = 3; // Invalid
                } else {
                    const bool inCheck = MoveGen::inCheck(pos);
                    MoveList moves;
                    if (inCheck)
                        MoveGen::checkEvasions(pos, moves);
                    else
                        MoveGen::pseudoLegalMoves(pos, moves);
                    bool hasLegalMoves = false;
                    for (int i = 0; i < moves.size; i++) {
                        if (MoveGen::isLegal(pos, moves[i], inCheck)) {
                            hasLegalMoves = true;
                            break;
                        }
                    }
                    if (!hasLegalMoves) {
                        wdl = 4; // Game end
                    } else {
                        int captWdl = wdlBestCapture(pos);
                        data[idx].setCaptureWdl(captWdl);
                        if (captWdl == (pos.isWhiteMove() ? 2 : -2)) {
                            wdl = captWdl;
                        } else {
                            int success;
                            wdl = Syzygy::probe_wdl(pos, &success);
                            if (!success)
                                throw ChessParseError("RTB probe failed, pos:" + TextIO::toFEN(pos));
                            if (pos.isWhiteMove()) {
                                bestWtm = std::max(bestWtm, wdl);
                            } else {
                                wdl = -wdl;
                                bestBtm = std::min(bestBtm, wdl);
                            }
                        }
                    }
                }
                data[idx].setWdl(wdl);
            }
            return std::make_pair(bestWtm, bestBtm);
        };
        pool.addTask(task);
        nTasks++;
    }
    std::cout << "nTasks:" << nTasks << std::endl;
    bestWtm = -2;
    bestBtm = 2;
    for (int i = 0; i < nTasks; i++) {
        std::pair<int,int> res;
        pool.getResult(res);
        bestWtm = std::max(bestWtm, res.first);
        bestBtm = std::min(bestBtm, res.second);
        if ((i+1) * 80 / nTasks > i * 80 / nTasks)
            std::cout << "." << std::flush;
    }
    std::cout << std::endl;
    std::cout << "bestWtm:" << bestWtm << " bestBtm:" << bestBtm << std::endl;
}

int
WdlCompress::wdlBestCapture(Position& pos) {
    const bool inCheck = MoveGen::inCheck(pos);
    MoveList moves;
    if (inCheck)
        MoveGen::checkEvasions(pos, moves);
    else
        MoveGen::pseudoLegalCaptures(pos, moves);
    int best = -2;
    for (int i = 0; i < moves.size; i++) {
        const Move& m = moves[i];
        if ((pos.getPiece(m.to()) == Piece::EMPTY) ||
            !MoveGen::isLegal(pos, m, inCheck))
            continue;
        UndoInfo ui;
        pos.makeMove(m, ui);
        int success;
        int wdl = -Syzygy::probe_wdl(pos, &success);
        if (!success)
            throw ChessParseError("RTB probe failed, pos:" + TextIO::toFEN(pos));
        pos.unMakeMove(m, ui);
        if (wdl > best) {
            best = wdl;
            if (best >= 2)
                break;
        }
    }
    return pos.isWhiteMove() ? best : -best;
}

void
WdlCompress::computeOptimalCaptures(std::vector<WDLInfo>& data) const {
    PosIndex& posIdx = *posIndex;
    const U64 size = posIdx.tbSize();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<int> pool(nThreads);
    int nTasks = 0;
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [this,&posIdx,&data,size,batchSize,b](int workerNo) {
            U64 end = std::min(b + batchSize, size);
            Position pos;
            for (U64 idx = b; idx < end; idx++) {
                if (data[idx].getWdl() == 3 || data[idx].getWdl() == 4)
                    continue;
                posIdx.index2Pos(idx, pos);
                int captWdl = data[idx].getCaptureWdl();
                if (captWdl == (pos.isWhiteMove() ? bestWtm : bestBtm))
                    data[idx].setWdl(5); // Optimal capture
            }
            return 0;
        };
        pool.addTask(task);
        nTasks++;
    }
    for (int i = 0; i < nTasks; i++) {
        int dummy;
        pool.getResult(dummy);
        if ((i+1) * 80 / nTasks > i * 80 / nTasks)
            std::cout << "." << std::flush;
    }
    std::cout << std::endl;
}

void
WdlCompress::computeStatistics(const std::vector<WDLInfo>& data,
                               std::array<U64,8>& cnt) const {
    const U64 size = data.size();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<std::array<U64,8>> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [&data,size,batchSize,b](int workerNo) {
            std::array<U64,8> cnt{};
            U64 end = std::min(b + batchSize, size);
            for (U64 idx = b; idx < end; idx++) {
                int val = data[idx].getWdl();
                cnt[val+2]++;
            }
            return cnt;
        };
        pool.addTask(task);
    }
    std::array<U64,8> res;
    while (pool.getResult(res))
        for (int i = 0; i < 8; i++)
            cnt[i] += res[i];

    std::cout << "header: -2 -1 0 1 2 invalid gameEnd optCapt" << std::endl;
    std::cout << "abs: ";
    for (int i = 0; i < 5; i++) {
        std::cout << cnt[i] << ' ';
    }
    std::cout << cnt[5] << ' ' << cnt[6] << ' ' << cnt[7] << std::endl;

    std::cout << "rel:";
    for (int i = 0; i < (int)cnt.size(); i++)
        std::cout << ' ' << std::setw(4) << (cnt[i] * 1000 + size/2) / size;
    std::cout << std::endl;

    std::cout << "invalid:" << (cnt[5] / (double)size) << std::endl;
    std::cout << "gameEnd:" << (cnt[6] / (double)size) << std::endl;
    std::cout << "optCapt:" << (cnt[7] / (double)size) << std::endl;
}

void
WdlCompress::replaceDontCares(std::vector<WDLInfo>& data, BitArray& active) {
    const U64 size = data.size();
    const U64 batchSize = std::max((U64)128*1024, ((size + 1023) / 1024) & ~63);
    ThreadPool<int> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [&data,&active,size,batchSize,b](int workerNo) {
            U64 end = std::min(b + batchSize, size);
            for (U64 idx = b; idx < end; idx++) {
                if (data[idx].getWdl() > 2) {
                    data[idx].setData(0);
                    active.set(idx, false);
                }
            }
            return 0;
        };
        pool.addTask(task);
    }
    int dummy;
    while (pool.getResult(dummy))
        ;
}

void
WdlCompress::writeFile(const std::vector<WDLInfo>& data,
                       const std::string& outFile) const {
    std::cout << "Writing..." << std::endl;
    std::ofstream outF(outFile);
    static_assert(sizeof(WDLInfo) == 1, "");
    outF.write((const char*)data.data(), data.size());
}
