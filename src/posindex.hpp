#ifndef POSINDEX_HPP_
#define POSINDEX_HPP_

#include "util/util.hpp"
#include <array>

class Position;

class PosIndex {
public:
    /** Constructor. */
    explicit PosIndex(const Position& pos);

    /** Return total number of positions in this tablebase class. */
    U64 tbSize() const;

    /** Return index in a tablebase corresponding to a position.
     *  "pos" is modified to correspond to the canonical symmetry position.
     *  Castling flags, full move counter and half-move clock are ignored.
     *  En passant square is updated in pos but ignored in the index calculation. */
    U64 pos2Index(Position& pos) const;

    /** Create a position corresponding to index. "pos" is assumed to be
     *  the empty board on input.
     *  If false is returned, index does not correspond to a valid position.
     *  If true is returned, the position can still be invalid if a king capture is possible. */
    bool index2Pos(U64 idx, Position& pos) const;

    static void staticInitialize();

private:

    static U64 binCoeff(int a, int b);

    static const int maxPieces = 8; // Max number of pieces
    static U64 binCoeffTable[64][maxPieces];

    /** For b pieces on a squares, there are binCoeff(a,b) combinations of occupied squares.
     *  Each combination has a unique index starting from 0. This method computes the mapping
     *  from index to the corresponding occupied squares. */
    void computeCombInverse(int a, int b, std::vector<U64>& vec) const;

    bool hasPawn;     // True if there is at least one pawn
    bool bwSymmetric; // If true, white/black have the same number of all piece types

    std::array<int,5> wPieces; // Q, R, B, N, P
    std::array<int,5> bPieces;

    int sideFactor;             // Number of choices for wtm/!wtm (1 or 2)
    int kingFactor;             // Number of king configurations
    std::array<int,5> wFactors; // Number of configurations for white piece types
    std::array<int,5> bFactors; // Number of configurations for black piece types

    // wCombInv[pieceTypeNo][idx] = bitboard of occupied squares corresponding to idx
    std::array<std::vector<U64>,5> wCombInv;
    std::array<std::vector<U64>,5> bCombInv;
};


// Number of legal king constellations for pawn/no pawn symmetry
const int nKingPawn   = 2*(64-4) + 12*(64-6) + 3*6*(64-9);
const int nKingNoPawn = 1*(36-3) + 3*(36-6) + 3*(64-6) + 3*(64-9);

class KingIndex {
public:
    /** Constructor. */
    explicit KingIndex(bool hasPawn);

    U64 index(int wKing, int bKing) const;

    /** Compute symmetry type corresponding to king positions. */
    int symmetryType(int wKing, int bKing) const;

    /** Remap a square given a symmetry type. */
    int symmetryRemap(int square, int symmetryType) const;

    /** Inverse of the index function. */
    void indexToKings(U64 idx, int& wKing, int& bKing) const;

    static void staticInitialize();

private:
    bool hasPawn;

    static int symmetryTable[8][64];         // [x*4+y*2+d][sq]
    static int indexTable[2][64][64];        // [hasPawn][wKing][bKing];
    static int symmetryTypeTable[2][64][64]; // [hasPawn][wKing][bKing];

    static int idxToKingNoPawn[nKingNoPawn]; // idx -> wKing*64+bKing
    static int idxToKingPawn[nKingPawn];
};


inline U64
PosIndex::binCoeff(int a, int b) {
    return binCoeffTable[a][b];
}


inline
KingIndex::KingIndex(bool hasPawn)
    : hasPawn(hasPawn) {
}

inline U64
KingIndex::index(int wKing, int bKing) const {
    return indexTable[hasPawn][wKing][bKing];
}

inline int
KingIndex::symmetryType(int wKing, int bKing) const {
    return symmetryTypeTable[hasPawn][wKing][bKing];
}

inline int
KingIndex::symmetryRemap(int square, int symmetryType) const {
    return symmetryTable[symmetryType][square];
}

inline void
KingIndex::indexToKings(U64 idx, int& wKing, int& bKing) const {
    int v = hasPawn ? idxToKingPawn[idx] : idxToKingNoPawn[idx];
    bKing = v & 63;
    wKing = v >> 6;
}

#endif /* POSINDEX_HPP_ */
