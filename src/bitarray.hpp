#ifndef BITARRAY_HPP_
#define BITARRAY_HPP_

#include "util/util.hpp"
#include <vector>


class BitArray {
public:
    BitArray(U64 size, bool initialVal);

    bool get(U64 idx) const;
    void set(U64 idx, bool val);

private:
    std::vector<U64> vec;
};


inline
BitArray::BitArray(U64 size, bool initialVal) {
    U64 len = (size + 63) / 64;
    U64 val = initialVal ? ~(0ULL) : 0;
    vec.assign(len, val);
}

inline bool
BitArray::get(U64 idx) const {
    return vec[idx/64] & (1ULL << (idx%64));
}

inline void
BitArray::set(U64 idx, bool val) {
    if (val)
        vec[idx/64] |= 1ULL << (idx%64);
    else
        vec[idx/64] &= ~(1ULL << (idx%64));
}

#endif /* BITARRAY_HPP_ */
