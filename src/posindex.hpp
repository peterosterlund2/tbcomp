#ifndef POSINDEX_HPP_
#define POSINDEX_HPP_

#include "util/util.hpp"
#include <array>

class Position;

class PosIndex {
public:
    /** Constructor. */
    PosIndex(const Position& pos);

    /** Return total number of positions in this tablebase class. */
    U64 tbSize() const;

    /** Return index in a tablebase corresponding to a position.
     *  "pos" is modified to correspond to the canonical symmetry position.
     *  Castling flags, full move counter and half-move clock are ignored.
     *  En passant square is updated in pos but ignored in the index calculation. */
    U64 pos2Index(Position& pos) const;

    /** Create a position corresponding to index. Return false
     *  if index does not correspond to a valid position. */
    bool index2Pos(U64 idx, Position& pos) const;

    static void staticInitialize();

private:

    static U64 binCoeff(int a, int b);

    static const int maxPieces = 8; // Max number of pieces
    static U64 binCoeffTable[64][maxPieces];

    std::array<int,5> wPieces; // Q, R, B, N, P
    std::array<int,5> bPieces;

    bool hasPawn;     // True if there is at least one pawn
    bool bwSwap;      // True if white/black was swapped e.g. because white had fewer pieces than black.
    bool bwSymmetric; // If true, white/black have the same number of all piece types

    int piecePos[maxPieces]; // Square for each piece
};



class KingIndex {
public:
    /** Constructor. */
    KingIndex(bool hasPawn);

    U64 index(int wKing, int bKing) const;

    /** Compute symmetry type corresponding to king positions. */
    int symmetryType(int wKing, int bKing) const;

    /** Remap a square given a symmetry type. */
    int symmetryRemap(int square, int symmetryType) const;

    static void staticInitialize();

private:
    bool hasPawn;

    static int symmetryTable[8][64];         // [x*4+y*2+d][sq]
    static int indexTable[2][64][64];        // [hasPawn][wKing][bKing];
    static int symmetryTypeTable[2][64][64]; // [hasPawn][wKing][bKing];
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

#endif /* POSINDEX_HPP_ */
