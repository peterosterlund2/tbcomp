#include "bitbuffer.hpp"

void BitBufferWriter::writeU64(U64 val) {
    U64 tmp = val;
    int nBits = 0;
    while (tmp > 0) {
        tmp /= 2;
        nBits++;
    }
    if (nBits > 0) {
        writeBits(0, nBits);
        writeBits(val, nBits);
    } else {
        writeBits(1, 1);
    }
}

U64 BitBufferReader::readU64() {
    int nBits = 0;
    while (!readBit())
        nBits++;
    U64 val = 0;
    if (nBits > 0) {
        val = readBits(nBits - 1);
        val |= 1ULL << (nBits - 1);
    }
    return val;
}
