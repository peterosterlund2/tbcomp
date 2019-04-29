#ifndef TBUTIL_HPP_
#define TBUTIL_HPP_

#include "util/util.hpp"
#include "bitbuffer.hpp"
#include <string>
#include <vector>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <cmath>

template<typename T> struct MakePrintable { static T convert(const T& v) { return v; } };
template<> struct MakePrintable<U8> { static int convert(const U8& v) { return v; } };

template<typename T>
std::ostream&
operator<<(std::ostream& os, const std::vector<T>& v) {
    os << "[";
    bool first = true;
    for (const T& e : v) {
        if (!first)
            os << ", ";
        os << MakePrintable<T>::convert(e);
        first = false;
    }
    os << " ]";
    return os;
}

inline void
printBits(BitBufferReader&& buf, U64 len) {
    for (U64 i = 0; i < len; i++) {
        if (i > 0) {
            if (i % 64 == 0)
                std::cout << '\n';
            else if (i % 8 == 0)
                std::cout << ' ';
        }
        bool val = buf.readBit();
        std::cout << (val ? '1' : '0');
    }
    std::cout << std::endl;
}

inline std::string
toBits(U64 val, int nBits) {
    std::stringstream ss;
    for (int i = nBits -1; i >= 0; i--) {
        bool b = ((1ULL << i) & val) != 0;
        ss << (b ? '1' : '0');
    }
    return ss.str();
}

/** "Scrambles" a 64 bit number. The sequence hashU64(i) for i=1,2,3,...
 *   passes "dieharder -a -Y 1". */
inline U64 hashU64(U64 v) {
    v *= 0x7CF9ADC6FE4A7653ULL;
    v ^= v >> 37;
    v *= 0xC25D3F49433E7607ULL;
    v ^= v >> 43;
    return v;
}

/** Compute entropy of a distribution, measured in number of bytes. */
template <typename Iter>
double entropy(Iter beg, Iter end) {
    U64 sum = std::accumulate(beg, end, (U64)0);
    if (sum == 0)
        return 0;
    double entr = 0;
    for (Iter it = beg; it != end; ++it) {
        double c = *it;
        if (c > 0) {
            entr += -c * std::log2(c / (double)sum);
        }
    }
    return entr / 8;
}

/** Compute estimated standard deviation of entropy assuming each count
 *  in the distribution was sampled from a Poisson distribution. */
template <typename Iter>
double entropyError(Iter beg, Iter end) {
    double sum = 0;
    for (Iter it = beg; it != end; ++it) {
        double v = *it ? *it : 1;
        sum += v;
    }
    double d = 0;
    for (Iter it = beg; it != end; ++it) {
        double v = *it ? *it : 1;
        double l = std::log2(v / sum);
        d += l * l * v;
    }
    return sqrt(d) / 8;
}


/** Compute Gini impurity of a distribution. */
template <typename Iter>
double giniImpurity(Iter beg, Iter end) {
    U64 sum = std::accumulate(beg, end, (U64)0);
    if (sum == 0)
        return 0;
    double iSum = 1.0 / sum;
    double gini = sum;
    for (Iter it = beg; it != end; ++it)
        gini -= iSum * (*it) * (*it);
    return gini;
}

/** Compute estimated standard deviation of Gini impurity assuming each count
 *  in the distribution was sampled from a Poisson distribution. */
template <typename Iter>
double giniImpurityError(Iter beg, Iter end) {
    double sum = 0;
    for (Iter it = beg; it != end; ++it) {
        double v = *it ? *it : 1;
        sum += v;
    }

    double sumP2 = 0;
    for (Iter it = beg; it != end; ++it) {
        double v = *it ? *it : 1;
        sumP2 += (v / sum) * (v / sum);
    }

    double d = 0;
    for (Iter it = beg; it != end; ++it) {
        double v = *it ? *it : 1;
        double tmp = 1 - 2*v/sum + sumP2;
        d += tmp * tmp * v;
    }
    return sqrt(d);
}


#endif /* TBUTIL_HPP_ */
