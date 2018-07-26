#ifndef REPAIR_HPP_
#define REPAIR_HPP_

#include "bitbuffer.hpp"


/** A recursively defined symbol used in the re-pair compression algorithm.
 * A symbol can be primitive, in which case it represents a single byte in
 * the uncompressed data.
 * A symbol can be composite, in which case it represents the concatenation
 * of two symbols. */
class RePairSymbol {
public:
    void setPrimitive(U16 sym);       // Set to a primitive symbol
    void setPair(U16 lSym, U16 rSym); // Set to a non-primitive symbol
    void setLength(U64 l, int d);     // Set symbol length to l

    bool isPrimitive() const;         // True if pair is not composed of two parts
    U16 getValue() const;             // For a primitive pair, get the symbol value

    U16 getLeft() const;
    U16 getRight() const;

    U64 getLength() const;

    int depth = 1;
private:
    U16 left;      // For a non primitive symbol, the left part of the pair
    U16 right;     // For a non primitive symbol, the right part of the pair
    U64 len;       // Total number of primitive symbols this symbol consists of
    const int INV = 0xffff;
};


/** Compress data using the recursive pairing (re-pair) algorithm.
 * Note that at most 65535 symbols are used. */
class RePairComp {
public:
    /** Run the re-pair algorithm. "inData" is overwritten.
     * "inData" must live longer than this object.
     * Uses inData.size()/8+O(1) extra memory. */
    RePairComp(std::vector<U8>& inData, int minFreq);

    /** Create compressed representation of the data. */
    void toBitBuf(BitBufferWriter& out);

private:
    void compress(U64 minFreq);

    bool getUsedIdx(U64 idx) const;
    void setUsedIdx(U64 idx, bool val);

    /** Return the symbol at index 'idx', or -1 if no symbol at that index. */
    int getData(U64 idx) const;

    std::vector<RePairSymbol> symbols;
    std::vector<U8>& data;
    /**
     * Each bit in usedIdx corresponds to one entry in data.
     * If the bit is 0, data[i] does not represent a symbol.
     * If the bit is 1 and the next bit is 1, the symbol is data[i]
     * If the bit is 1 and the next bit is 0, the symbol is data[i]+256*data[i+1]
     */
    std::vector<U64> usedIdx;
};

/** Decompress data previously compressed by RePairComp. */
class RePairDeComp {
public:
    /** Constructor. "inData" must live longer than this object. */
    RePairDeComp(const U8* inData);

    /** Decompress all data. */
    void deCompressAll(std::vector<U8>& outData);

    /** Decompress one data entry. */
    int operator[](U64 idx) const;

private:
    const U8* data;
};


inline void RePairSymbol::setPrimitive(U16 sym) {
    left = INV;
    right = sym;
    len = 1;
}

inline void RePairSymbol::setPair(U16 lSym, U16 rSym) {
    left = lSym;
    right = rSym;
}

inline void RePairSymbol::setLength(U64 l, int d) {
    len = l;
    depth = d;
}

inline bool RePairSymbol::isPrimitive() const {
    return left == INV;
}

inline U16 RePairSymbol::getValue() const {
    return right;
}

inline U16 RePairSymbol::getLeft() const {
    return left;
}

inline U16 RePairSymbol::getRight() const {
    return right;
}

inline U64 RePairSymbol::getLength() const {
    return len;
}


inline bool RePairComp::getUsedIdx(U64 idx) const {
    return usedIdx[idx/64] & (1ULL << (idx%64));
}

inline void RePairComp::setUsedIdx(U64 idx, bool val) {
    if (val)
        usedIdx[idx/64] |= 1ULL << (idx%64);
    else
        usedIdx[idx/64] &= ~(1ULL << (idx%64));
}

inline int
RePairComp::getData(U64 idx) const {
    if (getUsedIdx(idx)) {
        if ((idx+1 < data.size()) && !getUsedIdx(idx+1)) {
            return data[idx] + 256 * data[idx + 1];
        } else {
            return data[idx];
        }
    }
    return -1;
}


inline RePairDeComp::RePairDeComp(const U8* inData)
    : data(inData) {
}


#endif /* REPAIR_HPP_ */
