#include "permutator.hpp"
#include <cassert>
#include <iostream>


Permutator::Permutator(U64 N)
    : N(N) {
    assert(N > 0);
    assert(N <= (1ULL << 63));

    int nBits;
    if (N - 1 >= (1ULL << 32)) {
        nBits = 33 + floorLog2((N - 1) >> 32);
    } else {
        nBits = 1 + floorLog2(N - 1);
    }
    mask = (1ULL << nBits) - 1;

    c1 = (0x7CF9ADC6FE4A7653ULL >> (64 - nBits)) | 1;
    s1 = (37 * nBits / 64) | 1;
    c2 = (0xC25D3F49433E7607ULL >> (64 - nBits)) | 1;
    s2 = (43 * nBits / 64) | 1;

//    std::cout << "N:" << N << " nBits:" << nBits << " mask:" << num2Hex(mask) << std::endl;
//    std::cout << "c1:" << c1 << " s1:" << s1 << " c2:" << c2 << " s2:" << s2 << std::endl;
}
