#include "symbolarray.hpp"

SymbolArray::SymbolArray(std::vector<U8>& data, int chLogSize)
    : data(data) {
    U64 size = data.size();
    usedIdx.assign((size+1 + 63) / 64, ~(0ULL));

    if (chLogSize == -1) {
        chunkLogSize = 20;
        U64 chunkSize = 1ULL << chunkLogSize;
        while ((size + chunkSize - 1) / chunkSize > 1024) {
            chunkLogSize++;
            chunkSize *= 2;
        }
    } else {
        chunkLogSize = chLogSize;
    }
    U64 chunkSize = 1ULL << chunkLogSize;
    U64 beg = 0;
    for (int i = 0; beg < size; i++, beg += chunkSize) {
        Chunk c;
        c.beg = c.begUsed = beg;
        c.end = c.endUsed = std::min(size, beg + chunkSize);
        chunks.push_back(c);
    }
}
