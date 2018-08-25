#include "symbolarray.hpp"

SymbolArray::SymbolArray(std::vector<U8>& data, int chSize)
    : data(data) {
    U64 size = data.size();
    usedIdx.assign((size+1 + 63) / 64, ~(0ULL));

    if (chSize == -1) {
        chunkSize = 1ULL << 20;
        while ((size + chunkSize - 1) / chunkSize > 1024)
            chunkSize *= 2;
        chunkSize += 633 * 64; // To avoid cache conflict misses
    } else {
        chunkSize = chSize;
    }
    U64 beg = 0;
    for (int i = 0; beg < size; i++, beg += chunkSize) {
        Chunk c;
        c.beg = c.begUsed = beg;
        c.end = c.endUsed = std::min(size, beg + chunkSize);
        chunks.push_back(c);
    }
}
