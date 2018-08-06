#include "posindex.hpp"
#include "position.hpp"
#include <algorithm>
#include <cassert>



static StaticInitializer<PosIndex> evInit;

U64 PosIndex::binCoeffTable[64][7];

void
PosIndex::staticInitialize() {
    for (int a = 0; a < 64; a++) {
        U64 res = 1;
        for (int b = 0; b < 7; b++) {
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
}

U64
PosIndex::tbSize() const {
    U64 ret = bwSymmetric ? 1 : 2;

    const int nKingPawn   = 2*(64-4) + 12*(64-6) + 3*6*(64-9);
    const int nKingNoPawn = 1*(36-3) + 3*(36-6) + 3*(64-6) + 3*(64-9);

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

U64
PosIndex::pos2Index(const Position& pos) const {
//    assert(nWPieces + nBPieces + 2 <= maxPieces);

    return 0;
}

bool
PosIndex::index2Pos(U64 idx, Position& pos) const {
    return false;
}

