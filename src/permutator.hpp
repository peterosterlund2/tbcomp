#ifndef PERMUTATOR_HPP_
#define PERMUTATOR_HPP_

#include "util/util.hpp"

/**
 * Class to generate a random permutation of the integers 0 <= i < N.
 * Only a constant amount of memory is used and the permuted values can
 * "almost" be randomly accessed.
 */
class Permutator {
public:
    /** Create a permutation of the integers 0 <= i < N. */
    Permutator(U64 N);

    /** Get the i:th permuted value. The i:th value may be invalid, in
     *  which case i is automatically incremented to the next valid value.
     *  If i upon return from this method is >= maxIdx, the return value
     *  is invalid and there are no more elements in the permutation. */
    U64 permute(U64& i) const;

    /** Get limit for the input parameter to permute(). */
    U64 maxIdx() const;

private:
    U64 hashU64(U64 v) const;

    U64 N;
    U64 mask;
    U64 c1, c2;
    int s1, s2;
};

inline U64
Permutator::permute(U64& i) const {
    for ( ; i <= mask; i++) {
        U64 ret = hashU64(i);
        if (ret < N)
            return ret;
    }
    return 0;
}

inline U64
Permutator::maxIdx() const {
    return mask + 1;
}

inline U64
Permutator::hashU64(U64 v) const {
    v = (v * c1) & mask;
    v ^= v >> s1;
    v = (v * c2) & mask;
    v ^= v >> s2;
    return v;
}

#endif
