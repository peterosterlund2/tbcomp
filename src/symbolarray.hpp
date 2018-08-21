#ifndef SYMBOLARRAY_HPP_
#define SYMBOLARRAY_HPP_

#include "util/util.hpp"


/** Represents an array of symbols. Initial symbols are 1 byte long.
 *  Pairs of symbols can be replaced by new symbols, which can be one or two bytes long.
 *  The actual data storage is kept outside this class. */
class SymbolArray {
public:
    /** Construct symbol array from 1 byte symbols. */
    SymbolArray(std::vector<U8>& data, int chunkLogSize = -1);

    class iterator {
    public:
        /** Create iterator positioned at idx. */
        iterator(SymbolArray* arr, U64 idx);
        iterator(const iterator& it) = default;
        iterator& operator=(const iterator& it) = default;

        /** Move iterator to next symbol. Return true if success. */
        bool moveToNext();
        /** Move iterator to previous symbol. Return true if success. */
        bool moveToPrev();

        /** Get index corresponding to current position. */
        U64 getIndex() const;
        /** Return the symbol at current position, or -1 if no symbol at that index. */
        int getSymbol() const;
        /** Store symbol at current position and advance to next free location. */
        void putSymbol(int val);

    private:
        SymbolArray* arr;
        U64 idx;
        U64 begUsed; // Start of used range of current chunk
        U64 endUsed; // End of used range of current chunk
        int chunk;
    };

    struct Chunk {
        U64 beg, end;  // Chunk range is beg <= i < end, const after created
        U64 begUsed;   // Used range is begUsed <= i < endUsed, non-const
        U64 endUsed;
    };

    /** Get iterator positioned at idx. */
    iterator iter(U64 idx);

    /** Get chunk index corresponding to byte idx. */
    int getChunkIdx(U64 idx) const;
    /** Get the i:th chunk. */
    const std::vector<Chunk>& getChunks() const;

    /** Update the used range within a chunk.
     *  Used for quickly skipping unused parts of a chunk. */
    void setChunkUsedRange(int chunkNo, U64 begUsed, U64 endUsed);

    bool getUsedIdx(U64 idx) const;
    void setUsedIdx(U64 idx, bool val);

private:
    std::vector<U8>& data;
    /**
     * Each bit in usedIdx corresponds to one entry in data.
     * If the bit is 0, data[i] does not represent a symbol.
     * If the bit is 1 and the next bit is 1, the symbol is data[i]
     * If the bit is 1 and the next bit is 0, the symbol is data[i]+256*data[i+1]
     */
    std::vector<U64> usedIdx; // 1 bit per element in data[]
    int chunkLogSize;         // Common log2(end-beg) value (except for last chunk)

    std::vector<Chunk> chunks;
};

inline
SymbolArray::iterator::iterator(SymbolArray* arr, U64 idx)
    : arr(arr), idx(idx) {
    int chunkIdx = arr->getChunkIdx(idx);
    const Chunk& c = arr->getChunks()[chunkIdx];
    begUsed = c.begUsed;
    endUsed = c.endUsed;
    chunk = chunkIdx;
}

inline bool
SymbolArray::iterator::moveToNext() {
    while (true) {
        while (++idx < endUsed)
            if (arr->getUsedIdx(idx))
                return true;
        if (++chunk >= (int)arr->getChunks().size())
            return false;
        const Chunk& c = arr->getChunks()[chunk];
        begUsed = c.begUsed;
        endUsed = c.endUsed;
        idx = begUsed - 1;
    }
}

inline bool
SymbolArray::iterator::moveToPrev() {
    while (true) {
        while (idx-- > begUsed)
            if (arr->getUsedIdx(idx))
                return true;
        if (--chunk < 0)
            return false;
        const Chunk& c = arr->getChunks()[chunk];
        begUsed = c.begUsed;
        endUsed = c.endUsed;
        idx = endUsed;
    }
}

inline U64
SymbolArray::iterator::getIndex() const {
    return idx;
}

inline int
SymbolArray::iterator::getSymbol() const {
    if (arr->getUsedIdx(idx)) {
        int ret = arr->data[idx];
        if (!arr->getUsedIdx(idx + 1))
            ret += 256 * arr->data[idx + 1];
        return ret;
    }
    return -1;
}

inline void
SymbolArray::iterator::putSymbol(int val) {
    arr->data[idx] = (U8)(val & 0xff);
    arr->setUsedIdx(idx++, true);
    if (val >= 256) {
        arr->data[idx] = (U8)(val >> 8);
        arr->setUsedIdx(idx++, false);
    }
}

// ----------------------------------------------------------------

inline SymbolArray::iterator
SymbolArray::iter(U64 idx) {
    return iterator(this, idx);
}

inline int
SymbolArray::getChunkIdx(U64 idx) const {
    return idx >> chunkLogSize;
}

inline const std::vector<SymbolArray::Chunk>&
SymbolArray::getChunks() const {
    return chunks;
}

inline void
SymbolArray::setChunkUsedRange(int chunkNo, U64 begUsed, U64 endUsed) {
    Chunk& c = chunks[chunkNo];
    c.begUsed = begUsed;
    c.endUsed = endUsed;
}

inline bool
SymbolArray::getUsedIdx(U64 idx) const {
    return usedIdx[idx/64] & (1ULL << (idx%64));
}

inline void
SymbolArray::setUsedIdx(U64 idx, bool val) {
    if (val)
        usedIdx[idx/64] |= 1ULL << (idx%64);
    else
        usedIdx[idx/64] &= ~(1ULL << (idx%64));
}

#endif
