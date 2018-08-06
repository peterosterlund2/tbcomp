#include "posindex.hpp"
#include "position.hpp"
#include <algorithm>
#include <cassert>
#include <climits>


static StaticInitializer<PosIndex> posIndexInit;
static StaticInitializer<KingIndex> kingIndexInit;

// Number of legal king constellations for pawn/no pawn symmetry
const int nKingPawn   = 2*(64-4) + 12*(64-6) + 3*6*(64-9);
const int nKingNoPawn = 1*(36-3) + 3*(36-6) + 3*(64-6) + 3*(64-9);


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
}

U64
PosIndex::tbSize() const {
    U64 ret = bwSymmetric ? 1 : 2;

    ret *= hasPawn ? nKingPawn : nKingNoPawn;

    int np = 0;
    if (hasPawn) {
        ret *= binCoeff(48 - np, wPieces[4]);
        np += wPieces[4];
        ret *= binCoeff(48 - np, bPieces[4]);
        np += bPieces[4];
    }

    np += 2;
    for (int i = 0; i < 4; i++) {
        ret *= binCoeff(64 - np, wPieces[i]);
        np += wPieces[i];
        ret *= binCoeff(64 - np, bPieces[i]);
        np += bPieces[i];
    }

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
    ret = ret * (hasPawn ? nKingPawn : nKingNoPawn) + ki.index(wKing, bKing);
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

    occupied &= !BitBoard::maskRow1Row8;
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
    return false;
}


// ------------------------------------------------------------

int KingIndex::symmetryTable[8][64];
int KingIndex::indexTable[2][64][64];
int KingIndex::symmetryTypeTable[2][64][64];

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
                    indexTable[hasPawn][bestWKing][bestBKing] = idx++;
                }
                indexTable[hasPawn][wKing][bKing] = indexTable[hasPawn][bestWKing][bestBKing];
            }
        }
        assert(idx == (hasPawn ? nKingPawn : nKingNoPawn));
    }
}
