#ifndef PREDICATE_HPP_
#define PREDICATE_HPP_

#include <memory>
#include <cmath>

class Position;


class Predicate {
public:
    virtual ~Predicate() = default;

    /** Return true if predicate is true for "pos". */
    virtual bool eval(const Position& pos) const = 0;

    /** For debugging. */
    virtual std::string name() const = 0;
};


/** Compute entropy of a distribution, measured in number of bytes. */
template <typename Iter>
double entropy(Iter beg, Iter end) {
    double sum = 0;
    double entr = 0;
    for (Iter it = beg; it != end; ++it) {
        double c = *it;
        if (c > 0) {
            sum += c;
            entr += -c * std::log2(c);
        }
    }
    if (sum > 0)
        entr += sum * std::log2(sum);
    return entr / 8;
}


#endif /* PREDICATE_HPP_ */
