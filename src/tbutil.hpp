#ifndef TBUTIL_HPP_
#define TBUTIL_HPP_

#include "util/util.hpp"
#include "bitbuffer.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h>

template<typename T> struct MakePrintable { static T convert(const T& v) { return v; } };
template<> struct MakePrintable<U8> { static int convert(const U8& v) { return v; } };

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
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

#endif /* TBUTIL_HPP_ */
