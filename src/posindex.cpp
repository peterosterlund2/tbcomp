#include "posindex.hpp"
#include "position.hpp"
#include <algorithm>
#include <cassert>
#include <climits>


static StaticInitializer<PosIndex> posIndexInit;
static StaticInitializer<KingIndex> kingIndexInit;


U64 PosIndex::binCoeffTable[64][maxPieces];

void
PosIndex::staticInitialize() {
    for (int a = 0; a < 64; a++) {
        U64 res = 1;
        for (int b = 0; b < maxPieces; b++) {
            binCoeffTable[a][b] = res;
            res = res * (a - b) / (b+1);
        }
    }
}

PosIndex::PosIndex(const Position& pos) {
    wPieces[0] = BitBoard::bitCount(pos.pieceTypeBB(Piece::WQUEEN));
    bPieces[0] = BitBoard::bitCount(pos.pieceTypeBB(Piece::BQUEEN));

    wPieces[1] = BitBoard::bitCount(pos.pieceTypeBB(Piece::WROOK));
    bPieces[1] = BitBoard::bitCount(pos.pieceTypeBB(Piece::BROOK));

    wPieces[2] = BitBoard::bitCount(pos.pieceTypeBB(Piece::WBISHOP));
    bPieces[2] = BitBoard::bitCount(pos.pieceTypeBB(Piece::BBISHOP));

    wPieces[3] = BitBoard::bitCount(pos.pieceTypeBB(Piece::WKNIGHT));
    bPieces[3] = BitBoard::bitCount(pos.pieceTypeBB(Piece::BKNIGHT));

    wPieces[4] = BitBoard::bitCount(pos.pieceTypeBB(Piece::WPAWN));
    bPieces[4] = BitBoard::bitCount(pos.pieceTypeBB(Piece::BPAWN));

    int np = 16;
    wFactors[4] = binCoeff(64 - np, wPieces[4]);
    computeCombInverse(64 - np, wPieces[4], wCombInv[4]);
    np += wPieces[4];

    bFactors[4] = binCoeff(64 - np, bPieces[4]);
    computeCombInverse(64 - np, bPieces[4], bCombInv[4]);
    np += bPieces[4];

    np = np - 16 + 2;
    for (int i = 3; i >= 0; i--) {
        wFactors[i] = binCoeff(64 - np, wPieces[i]);
        computeCombInverse(64 - np, wPieces[i], wCombInv[i]);
        np += wPieces[i];

        bFactors[i] = binCoeff(64 - np, bPieces[i]);
        computeCombInverse(64 - np, bPieces[i], bCombInv[i]);
        np += bPieces[i];
    }

    bwSwap = false;
    int nWPieces = wPieces[0] + wPieces[1] + wPieces[2] + wPieces[3] + wPieces[4];
    int nBPieces = bPieces[0] + bPieces[1] + bPieces[2] + bPieces[3] + bPieces[4];
    assert(nWPieces + nBPieces + 2 <= maxPieces);
    if (nWPieces < nBPieces) {
        bwSwap = true;
    } else if (nWPieces == nBPieces) {
        if (std::lexicographical_compare(wPieces.begin(), wPieces.end(),
                                         bPieces.begin(), bPieces.end()))
            bwSwap = true;
    }

    if (bwSwap)
        std::swap(wPieces, bPieces);
    hasPawn = wPieces[4] + bPieces[4] > 0;
    bwSymmetric = wPieces == bPieces;
    assert(!(bwSwap && bwSymmetric));

    sideFactor = bwSymmetric ? 1 : 2;
    kingFactor = hasPawn ? nKingPawn : nKingNoPawn;
}

U64
PosIndex::tbSize() const {
    U64 ret = sideFactor * kingFactor;
    for (int i = 0; i < 5; i++)
        ret *= wFactors[i] * bFactors[i];
    return ret;
}

static Position swapColors(const Position& pos) {
    Position sym;
    sym.setWhiteMove(!pos.isWhiteMove());
    for (int sq = 0; sq < 64; sq++) {
        int p = pos.getPiece(sq);
        p = Piece::isWhite(p) ? Piece::makeBlack(p) : Piece::makeWhite(p);
        sym.setPiece(Square::mirrorY(sq), p);
    }

    if (pos.getEpSquare() >= 0)
        sym.setEpSquare(Square::mirrorY(pos.getEpSquare()));

    return sym;
}

U64
PosIndex::pos2Index(Position& pos) const {
    if (bwSwap || (bwSymmetric && !pos.isWhiteMove()))
        pos = swapColors(pos);

    U64 ret = pos.isWhiteMove() ? 0 : 1;

    int wKing = pos.getKingSq(true);
    int bKing = pos.getKingSq(false);

    KingIndex ki(hasPawn);
    ret = ret * kingFactor + ki.index(wKing, bKing);
    int symType = ki.symmetryType(wKing, bKing);

    Position symPos;
    symPos.setPiece(ki.symmetryRemap(wKing, symType), Piece::WKING);
    symPos.setPiece(ki.symmetryRemap(bKing, symType), Piece::BKING);

    for (int pt = Piece::WKING; pt <= Piece::BPAWN; pt++) {
        U64 m = pos.pieceTypeBB((Piece::Type)pt);
        while (m) {
            int sq = BitBoard::extractSquare(m);
            symPos.setPiece(ki.symmetryRemap(sq, symType), pt);
        }
    }
    if (pos.getEpSquare() >= 0)
        symPos.setEpSquare(ki.symmetryRemap(pos.getEpSquare(), symType));
    pos = symPos;

    U64 occupied = BitBoard::maskRow1Row8;

    auto addIndex = [&pos,&occupied,&ret](Piece::Type pt) {
        int nSq = 64 - BitBoard::bitCount(occupied);
        U64 mask = pos.pieceTypeBB(pt);
        int np = BitBoard::bitCount(mask);
        U64 idx = 0;
        U64 m = mask;
        for (int i = 0; i < np; i++) {
            int sq = BitBoard::extractSquare(m);
            sq -= BitBoard::bitCount(((1ULL << sq) - 1) & occupied);
            idx += binCoeff(sq, i+1);
        }
        occupied |= mask;
        ret = ret * binCoeff(nSq, np) + idx;
    };

    addIndex(Piece::WPAWN);
    addIndex(Piece::BPAWN);

    occupied &= ~BitBoard::maskRow1Row8;
    occupied |= pos.pieceTypeBB(Piece::WKING, Piece::BKING);

    addIndex(Piece::WKNIGHT);
    addIndex(Piece::BKNIGHT);
    addIndex(Piece::WBISHOP);
    addIndex(Piece::BBISHOP);
    addIndex(Piece::WROOK);
    addIndex(Piece::BROOK);
    addIndex(Piece::WQUEEN);
    addIndex(Piece::BQUEEN);

    return ret;
}

bool
PosIndex::index2Pos(U64 idx, Position& pos) const {
    std::array<int,5> wIdx, bIdx;

    for (int i = 0; i < 5; i++) {
        int f = bFactors[i];
        bIdx[i] = idx % f; idx /= f;
        f = wFactors[i];
        wIdx[i] = idx % f; idx /= f;
    }

    int kingIdx = idx % kingFactor;

    int side = 0;
    if (sideFactor > 1) {
        idx /= kingFactor;
        side = idx;
    }
    pos.setWhiteMove(side == 0);

    KingIndex ki(hasPawn);
    int wKing, bKing;
    ki.indexToKings(kingIdx, wKing, bKing);
    pos.setPiece(wKing, Piece::WKING);
    pos.setPiece(bKing, Piece::BKING);

    U64 occupied = BitBoard::maskRow1Row8;

    auto placePieces = [&pos,&occupied](Piece::Type pt, U64 mask, bool pawn) {
        U64 newMask = 0;
        while (mask) {
            int sq0 = BitBoard::extractSquare(mask);
            int sq = sq0 + (pawn ? 8 : 0);
            while (true) {
                int tmp = sq0 + BitBoard::bitCount(occupied & ((1ULL<<(sq+1))-1));
                if (tmp == sq)
                    break;
                sq = tmp;
            }
            pos.setPiece(sq, pt);
            newMask |= 1ULL << sq;
        }
        occupied |= newMask;
    };

    placePieces(Piece::WPAWN, wCombInv[4][wIdx[4]], true);
    placePieces(Piece::BPAWN, bCombInv[4][bIdx[4]], true);

    if (pos.pieceTypeBB(Piece::WPAWN, Piece::BPAWN) &
            BitBoard::sqMask((SquareName)wKing, (SquareName)bKing))
        return false;

    occupied &= ~BitBoard::maskRow1Row8;
    occupied |= pos.pieceTypeBB(Piece::WKING, Piece::BKING);

    placePieces(Piece::WKNIGHT, wCombInv[3][wIdx[3]], false);
    placePieces(Piece::BKNIGHT, bCombInv[3][bIdx[3]], false);
    placePieces(Piece::WBISHOP, wCombInv[2][wIdx[2]], false);
    placePieces(Piece::BBISHOP, bCombInv[2][bIdx[2]], false);
    placePieces(Piece::WROOK,   wCombInv[1][wIdx[1]], false);
    placePieces(Piece::BROOK,   bCombInv[1][bIdx[1]], false);
    placePieces(Piece::WQUEEN,  wCombInv[0][wIdx[0]], false);
    placePieces(Piece::BQUEEN,  bCombInv[0][bIdx[0]], false);

    return true;
}

void PosIndex::computeCombInverse(int a, int b, std::vector<U64>& vec) const {
    U64 squares = (1ULL << b) - 1;
    U64 last = squares << (a - b);

    while (true) {
        vec.push_back(squares);
        if (squares == last)
            break;
        int nOnes = 0;
        for (int i = 0; i < a; i++) {
            if ((squares & (1ULL << i)) && !(squares & (1ULL << (i+1)))) {
                squares |= 1ULL << (i+1);
                squares &= ~((1ULL << (i+1)) - 1);
                squares |= (1ULL << nOnes) - 1;
                break;
            }
            if (squares & (1ULL << i))
                nOnes++;
        }
    }

    assert(vec.size() == binCoeff(a, b));
}

// ------------------------------------------------------------

int KingIndex::symmetryTable[8][64];
int KingIndex::indexTable[2][64][64];
int KingIndex::symmetryTypeTable[2][64][64];

int KingIndex::idxToKingNoPawn[nKingNoPawn];
int KingIndex::idxToKingPawn[nKingPawn];

void
KingIndex::staticInitialize() {
    for (int xMirror = 0; xMirror < 2; xMirror++) {
        for (int yMirror = 0; yMirror < 2; yMirror++) {
            for (int dMirror = 0; dMirror < 2; dMirror++) {
                int symmetryType = xMirror * 4 + yMirror * 2 + dMirror;
                for (int sq = 0; sq < 64; sq++) {
                    int x = sq % 8;
                    int y = sq / 8;
                    if (xMirror) {
                        x = 7 - x;
                    }
                    if (yMirror) {
                        y = 7 - y;
                    }
                    if (dMirror) {
                        std::swap(x, y);
                    }
                    symmetryTable[symmetryType][sq] = y * 8 + x;
                }
            }
        }
    }

    for (int hasPawn = 0; hasPawn < 2; hasPawn++) {
        for (int wKing = 0; wKing < 64; wKing++) {
            for (int bKing = 0; bKing < 64; bKing++) {
                indexTable[hasPawn][wKing][bKing] = -1;
                symmetryTypeTable[hasPawn][wKing][bKing] = -1;
            }
        }
    }

    for (int hasPawn = 0; hasPawn < 2; hasPawn++) {
        int idx = 0;
        for (int wKing = 0; wKing < 64; wKing++) {
            for (int bKing = 0; bKing < 64; bKing++) {
                {
                    int wx = wKing % 8;
                    int wy = wKing / 8;
                    int bx = bKing % 8;
                    int by = bKing / 8;
                    if ((abs(wx - bx) <= 1) && (abs(wy - by) <= 1))
                        continue;
                }
                int bestSymmetry = -1;
                int bestScore = INT_MAX;
                for (int symmetryType = 0; symmetryType < 8; symmetryType++) {
                    if (hasPawn && (symmetryType != 0) && (symmetryType != 4))
                        continue;
                    int w = symmetryTable[symmetryType][wKing];
                    int b = symmetryTable[symmetryType][bKing];
                    int score = w * 64 + b;
                    if (score < bestScore) {
                        bestSymmetry = symmetryType;
                        bestScore = score;
                    }
                }
                assert(bestSymmetry != -1);
                symmetryTypeTable[hasPawn][wKing][bKing] = bestSymmetry;
                int bestWKing = bestScore / 64;
                int bestBKing = bestScore % 64;
                if (indexTable[hasPawn][bestWKing][bestBKing] == -1) {
                    indexTable[hasPawn][bestWKing][bestBKing] = idx;
                    (hasPawn ? idxToKingPawn : idxToKingNoPawn)[idx] = bestScore;
                    idx++;
                }
                indexTable[hasPawn][wKing][bKing] = indexTable[hasPawn][bestWKing][bestBKing];
            }
        }
        assert(idx == (hasPawn ? nKingPawn : nKingNoPawn));
    }
}
