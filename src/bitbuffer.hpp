#ifndef BITBUFFER_HPP_
#define BITBUFFER_HPP_

#include "util/util.hpp"


class BitBufferWriter {
public:
    /** Constructor. */
    BitBufferWriter();

    /** Store "nBits" bits in the buffer, defined by the least
     *  significant bits in "val". The other bits in "val" must be 0.
     *  The bits are stored in big-endian order.
     *  @pre nBits < 64 */
    void writeBits(U64 val, int nBits);

    /** Store a value of unspecified size. Small values use fewer bits than large values. */
    void writeU64(U64 val);

    /** Return total number of written bits. */
    U64 getNumBits() const { return buf.size() * 8 + nDataBits; }

    /** Return the underlying data buffer.
     *  Must be called only once when all data has been written. */
    const std::vector<U8>& getBuf();

private:
    void writeData();

    std::vector<U8> buf;
    U64 data;
    int nDataBits;
};


class BitBufferReader {
public:
    /** Constructor. */
    explicit BitBufferReader(const U8* buf);

    /** Return the next "nBits" bits.
     *  The bits are read in big-endian order.
     *  @pre nBits < 64 */
    U64 readBits(int nBits);

    /** Return value stored by BitBufferWriter::writeU64(). */
    U64 readU64();

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
    : data(0), nDataBits(0) {
}

inline void
BitBufferWriter::writeBits(U64 val, int nBits) {
    if (nDataBits + nBits < 64) {
        data <<= nBits;
        data |= val;
        nDataBits += nBits;
    } else {
        data <<= 64 - nDataBits;
        nDataBits += nBits - 64;
        data |= val >> nDataBits;
        writeData();
        data = val;
    }
}

inline const std::vector<U8>&
BitBufferWriter::getBuf() {
    if (nDataBits > 0) {
        data <<= 64 - nDataBits;
        writeData();
    }
    return buf;
}

inline BitBufferReader::BitBufferReader(const U8* buf)
    : buf(buf), data(0), nDataBits(0) {
}

inline U64
BitBufferReader::readBits(int nBits) {
    U64 ret = 0;
    int lastBits = nBits;
    if (nBits > nDataBits) {
        lastBits -= nDataBits;
        ret = data >> (64 - nDataBits);
        ret <<= lastBits;
        readData();
    }
    if (nBits > 0) {
        ret |= data >> (64 - lastBits);
        data <<= lastBits;
        nDataBits -= nBits;
    }
    return ret;
}

inline bool
BitBufferReader::readBit() {
    if (nDataBits == 0)
        readData();
    bool ret = (data & (1ULL << 63)) != 0;
    nDataBits--;
    data <<= 1;
    return ret;
}

inline void
BitBufferReader::readData() {
    const U8* ptr = buf;
    U64 val = (*ptr++);
    val |= (U64)(*ptr++) << (8*1);
    val |= (U64)(*ptr++) << (8*2);
    val |= (U64)(*ptr++) << (8*3);
    val |= (U64)(*ptr++) << (8*4);
    val |= (U64)(*ptr++) << (8*5);
    val |= (U64)(*ptr++) << (8*6);
    val |= (U64)(*ptr++) << (8*7);
    buf = ptr;
    data = val;
    nDataBits += 64;
}

#endif /* BITBUFFER_HPP_ */
