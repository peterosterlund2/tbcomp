#ifndef REPAIR_HPP_
#define REPAIR_HPP_

#include "bitbuffer.hpp"
#include <unordered_map>

namespace RePairImpl {
    struct CompressData;
    struct DeltaFreq;
}

/** A recursively defined symbol used in the re-pair compression algorithm.
 * A symbol can be primitive, in which case it represents a single byte in
 * the uncompressed data.
 * A symbol can be composite, in which case it represents the concatenation
 * of two symbols. */
class RePairSymbol {
public:
    void setPrimitive(U16 sym);        // Set to a primitive symbol
    void setPair(U16 lSym, U16 rSym);  // Set to a non-primitive symbol
    void setLengthDepth(U64 l, int d); // Set symbol length to l and depth to d

    bool isPrimitive() const;          // True if pair is not composed of two parts
    U16 getValue() const;              // For a primitive pair, get the symbol value

    U16 getLeft() const;
    U16 getRight() const;

    U64 getLength() const;
    int getDepth() const;

private:
    U16 left;      // For a non primitive symbol, the left part of the pair
    U16 right;     // For a non primitive symbol, the right part of the pair
    U64 len;       // Total number of primitive symbols this symbol consists of
    int depth;
    static const int INV = 0xffff;
};


/** Compress data using the recursive pairing (re-pair) algorithm.
 * Note that at most 65535 symbols are used. */
class RePairComp {
public:
    /** Run the re-pair algorithm. "inData" is overwritten.
     * "inData" must live longer than this object.
     * Uses inData.size()/8+O(1) extra memory. */
    RePairComp(std::vector<U8>& inData, int minFreq, int maxSyms);

    /** Create compressed representation of the data. */
    void toBitBuf(BitBufferWriter& out);

private:
    void compress(U64 minFreq, int maxSyms);
    void initSymbols(RePairImpl::CompressData& cpData);
    void pruneCache(RePairImpl::CompressData& cpData, S64 maxSize, U64 maxFreq) const;
    void refillCache(RePairImpl::CompressData& cpData, U64 maxCache);
    U64 replacePairs(const std::vector<U64>& indices, int X, int Y, int Z,
                     RePairImpl::DeltaFreq& delta);

    bool getUsedIdx(U64 idx) const;
    void setUsedIdx(U64 idx, bool val);

    /** Return the symbol at index 'idx', or -1 if no symbol at that index. */
    int getData(U64 idx) const;
    /** Return symbol at "idx" and advance idx to next symbol. */
    int getNextSymbol(U64& idx) const;
    /** Return symbol before "idx" and retreat idx to previous symbol. */
    int getPrevSymbol(U64& idx) const;

    std::vector<RePairSymbol> symbols;
    std::vector<U8>& data;
    /**
     * Each bit in usedIdx corresponds to one entry in data.
     * If the bit is 0, data[i] does not represent a symbol.
     * If the bit is 1 and the next bit is 1, the symbol is data[i]
     * If the bit is 1 and the next bit is 0, the symbol is data[i]+256*data[i+1]
     */
    std::vector<U64> usedIdx;
    int nThreads;
};

/** Decompress data previously compressed by RePairComp. */
class RePairDeComp {
public:
    /** Constructor. "inData" must live longer than this object. */
    explicit RePairDeComp(const U8* inData);

    /** Decompress all data. */
    void deCompressAll(std::function<void(const std::vector<U8>&)> consumer);

    /** Decompress one data entry. */
    int operator[](U64 idx) const;

private:
    const U8* data;
};

/** Fast mapping from integer to cache data. */
class LookupTable {
public:
    /** Constructor */
    explicit LookupTable(std::unordered_map<U32,std::vector<U64>>& data);

    /** Return data corresponding to key, or nullptr if not found. */
    std::vector<U64>* lookup(U32 key) const;

private:
    U32 hashVal(U32 key) const {
        key *= 2654435789UL;
        key = key ^ (key >> 19);
        return key;
    }

    struct Entry {
        U32 key = 0;
        bool empty = true;
        std::vector<U64>* value = nullptr;
    };
    std::vector<Entry> table;
    U32 mask;
};

inline void RePairSymbol::setPrimitive(U16 sym) {
    left = INV;
    right = sym;
    len = 1;
    depth = 1;
}

inline void RePairSymbol::setPair(U16 lSym, U16 rSym) {
    left = lSym;
    right = rSym;
}

inline void RePairSymbol::setLengthDepth(U64 l, int d) {
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

inline int RePairSymbol::getDepth() const {
    return depth;
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
        int ret = data[idx];
        if (!getUsedIdx(idx+1))
            ret += 256 * data[idx + 1];
        return ret;
    }
    return -1;
}

inline int
RePairComp::getNextSymbol(U64& idx) const {
    if (idx >= data.size())
        return -1;
    int ret = getData(idx);
    while (++idx < data.size() && !getUsedIdx(idx))
        ;
    return ret;
}

inline int
RePairComp::getPrevSymbol(U64& idx) const {
    if (idx == 0)
        return -1;
    while (true) {
        idx--;
        if (getUsedIdx(idx))
            return getData(idx);
        if (idx == 0)
            return -1;
    }
}

inline RePairDeComp::RePairDeComp(const U8* inData)
    : data(inData) {
}


#endif /* REPAIR_HPP_ */
