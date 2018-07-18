#ifndef BITBUFFER_HPP_
#define BITBUFFER_HPP_

#include "types.hpp"
#include <vector>


class BitBufferWriter {
public:
    BitBufferWriter();

    /** Store "nBits" bits in the buffer, defined by the least
     *  significant bits in "val". The other bits in "val" must be 0.
     *  @pre nBits < 64 */
    void putBits(U64 val, int nBits);

    /** Return the underlying data buffer. Must not be called more than once. */
    const std::vector<U64>& getBuf();

private:
    std::vector<U64> buf;
    U64 data;
    int bitOffs;
};


class BitBufferReader {
public:
    BitBufferReader(const U8* buf);

    /** Return the next "nBits" bits.
     *  @pre nBits < 64 */
    U64 readBits(int nBits);

    /** Return the next bit. */
    bool readBit();

private:
    /** Read next 64 bits from buf. */
    void readData();

    const U8* buf;
    U64 data;
    int nDataBits; // Number of valid bits in data
};


inline BitBufferWriter::BitBufferWriter()
    : data(0), bitOffs(0) {
}

inline void
BitBufferWriter::putBits(U64 val, int nBits) {
    data |= val << bitOffs;
    bitOffs += nBits;
    if (bitOffs >= 64) {
        buf.push_back(data);
        bitOffs -= 64;
        val >>= nBits - bitOffs;
        data = val;
    }
}

inline const std::vector<U64>&
BitBufferWriter::getBuf() {
    if (bitOffs > 0)
        buf.push_back(data);
    return buf;
}

inline BitBufferReader::BitBufferReader(const U8* buf)
    : buf(buf), data(0), nDataBits(0) {
}

inline U64
BitBufferReader::readBits(int nBits) {
    U64 ret = data;
    int nFirstPart = 0;
    if (nBits > nDataBits) {
        nFirstPart = nDataBits;
        readData();
        ret |= data << nFirstPart;
    }
    nDataBits -= nBits;
    data >>= nBits - nFirstPart;
    ret &= (1ULL << nBits) - 1;
    return ret;
}

inline bool
BitBufferReader::readBit() {
    if (nDataBits == 0)
        readData();
    bool ret = data & 1;
    nDataBits--;
    data >>= 1;
    return ret;
}

inline void
BitBufferReader::readData() {
    U64 val = (*buf++);
    val |= (U64)(*buf++) << (8*1);
    val |= (U64)(*buf++) << (8*2);
    val |= (U64)(*buf++) << (8*3);
    val |= (U64)(*buf++) << (8*4);
    val |= (U64)(*buf++) << (8*5);
    val |= (U64)(*buf++) << (8*6);
    val |= (U64)(*buf++) << (8*7);
    data = val;
    nDataBits += 64;
}

#endif /* BITBUFFER_HPP_ */
