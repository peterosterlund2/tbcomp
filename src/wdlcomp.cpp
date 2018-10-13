#include "wdlcomp.hpp"
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

WdlCompress::WdlCompress(const std::string& tbType) {
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

void WdlCompress::wdlDump(const std::string& outFile) {
    PosIndex& posIdx = *posIndex;
    const U64 size = posIdx.tbSize();
    std::vector<U8> data(size);

    initializeData(data);
    computeOptimalCaptures(data);

    std::array<U64,8> cnt{};
    int mostFreq = computeStatistics(data, cnt);
    replaceDontCares(data, mostFreq);

    writeFile(data, outFile);
}

void WdlCompress::initializeData(std::vector<U8>& data) {
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
                for (U64 m = pos.occupiedBB(); m; ) // Clear position
                    pos.clearPiece(BitBoard::extractSquare(m));
                bool valid = posIdx.index2Pos(idx, pos);
                if (valid && MoveGen::canTakeKing(pos))
                    valid = false;
                int val;
                if (!valid) {
                    val = 3; // Invalid
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
                        val = 4; // Game end
                    } else {
                        int success;
                        int wdl = Syzygy::probe_wdl(pos, &success);
                        if (!success)
                            throw ChessParseError("RTB probe failed, pos:" + TextIO::toFEN(pos));
                        if (pos.isWhiteMove()) {
                            bestWtm = std::max(bestWtm, wdl);
                        } else {
                            wdl = -wdl;
                            bestBtm = std::min(bestBtm, wdl);
                        }
                        val = wdl;
                    }
                }
                data[idx] = (U8)val;
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

void WdlCompress::computeOptimalCaptures(std::vector<U8>& data) const {
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
                if (data[idx] == 3 || data[idx] == 4)
                    continue;
                for (U64 m = pos.occupiedBB(); m; ) // Clear position
                    pos.clearPiece(BitBoard::extractSquare(m));
                posIdx.index2Pos(idx, pos);
                int success;
                UndoInfo ui;
                bool optCapt = false;
                const bool inCheck = MoveGen::inCheck(pos);
                MoveList moves;
                if (inCheck)
                    MoveGen::checkEvasions(pos, moves);
                else
                    MoveGen::pseudoLegalMoves(pos, moves);
                for (int i = 0; i < moves.size; i++) {
                    const Move& m = moves[i];
                    if ((pos.getPiece(m.to()) == Piece::EMPTY) ||
                            !MoveGen::isLegal(pos, m, inCheck))
                        continue;
                    pos.makeMove(m, ui);
                    int wdl = -Syzygy::probe_wdl(pos, &success);
                    if (!success)
                        throw ChessParseError("RTB probe failed, pos:" + TextIO::toFEN(pos));
                    pos.unMakeMove(m, ui);
                    if (pos.isWhiteMove()) {
                        if (wdl == bestWtm) {
                            optCapt = true;
                            break;
                        }
                    } else {
                        wdl = -wdl;
                        if (wdl == bestBtm) {
                            optCapt = true;
                            break;
                        }
                    }
                }
                if (optCapt)
                    data[idx] = 5;
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

int WdlCompress::computeStatistics(const std::vector<U8>& data,
                                   std::array<U64,8>& cnt) const {
    const U64 size = data.size();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<std::array<U64,8>> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [&data,size,batchSize,b](int workerNo) {
            std::array<U64,8> cnt{};
            U64 end = std::min(b + batchSize, size);
            for (U64 idx = b; idx < end; idx++) {
                int val = (S8)data[idx];
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

    std::cout << "header: -2 -1 0 1 2 illegal gameEnd optCapt" << std::endl;
    std::cout << "abs: ";
    int mostFreq = 0;
    for (int i = 0; i < 5; i++) {
        std::cout << cnt[i] << ' ';
        if (cnt[i] > cnt[mostFreq])
            mostFreq = i;
    }
    std::cout << cnt[5] << ' ' << cnt[6] << ' ' << cnt[7] << std::endl;

    std::cout << "rel:";
    for (int i = 0; i < (int)cnt.size(); i++)
        std::cout << ' ' << std::setw(4) << (cnt[i] * 1000 + size/2) / size;
    std::cout << std::endl;

    std::cout << "invalid:" << (cnt[5] / (double)size) << std::endl;
    std::cout << "gameEnd:" << (cnt[6] / (double)size) << std::endl;
    std::cout << "optCapt:" << (cnt[7] / (double)size) << std::endl;

    return mostFreq;
}

void WdlCompress::replaceDontCares(std::vector<U8>& data, int mostFreq) {
    const U64 size = data.size();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<int> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [&data,mostFreq,size,batchSize,b](int workerNo) {
            U64 end = std::min(b + batchSize, size);
            for (U64 idx = b; idx < end; idx++) {
                if (data[idx] > 2 && data[idx] < 128)
                    data[idx] = (U8)(mostFreq-2);
            }
            return 0;
        };
        pool.addTask(task);
    }
    int dummy;
    while (pool.getResult(dummy))
        ;
}

void WdlCompress::writeFile(const std::vector<U8>& data,
                            const std::string& outFile) const {
    std::ofstream outF(outFile);
    outF.write((const char*)data.data(), data.size());
}
