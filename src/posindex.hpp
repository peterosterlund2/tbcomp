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

    /** Return index in a tablebase corresponding to a position. */
    U64 pos2Index(const Position& pos) const;

    /** Create a position corresponding to index. Return false
     *  if index does not correspond to a valid position. */
    bool index2Pos(U64 idx, Position& pos) const;

    static void staticInitialize();

private:

    static U64 binCoeff(int a, int b);

    static U64 binCoeffTable[64][7];


    static const int maxPieces = 8; // Max number of pieces

    std::array<int,5> wPieces; // Q, R, B, N, P
    std::array<int,5> bPieces;

    bool hasPawn;     // True if there is at least one pawn
    bool bwSwap;      // True if white/black was swapped e.g. because white had fewer pieces than black.
    bool bwSymmetric; // If true, white/black have the same number of all piece types

    int piecePos[maxPieces]; // Square for each piece
};

inline U64
PosIndex::binCoeff(int a, int b) {
    return binCoeffTable[a][b];
}


#endif /* POSINDEX_HPP_ */
