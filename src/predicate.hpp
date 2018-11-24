#ifndef PREDICATE_HPP_
#define PREDICATE_HPP_

#include "util/util.hpp"
#include <memory>
#include <numeric>
#include <cmath>

class Position;
namespace DT {
    class EvalContext;
}


class Predicate {
public:
    virtual ~Predicate() = default;

    /** Return true if predicate is true for "pos". */
    virtual bool eval(const Position& pos, DT::EvalContext& ctx) const = 0;

    /** For debugging. */
    virtual std::string name() const = 0;
};


/** Compute entropy of a distribution, measured in number of bytes. */
template <typename Iter>
double entropy(Iter beg, Iter end) {
    U64 sum = std::accumulate(beg, end, 0);
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


#endif /* PREDICATE_HPP_ */
