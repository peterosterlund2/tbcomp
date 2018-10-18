#ifndef PREDICATE_HPP_
#define PREDICATE_HPP_

#include <memory>
#include <cmath>

class Position;


class Predicate {
public:
    virtual ~Predicate() = default;

    enum ApplyResult {
        PRED_TRUE,
        PRED_FALSE,
        PRED_NONE,
    };

    /** For a non-leaf node, return whether predicate is true.
     *  For a leaf node, update statistics for all potential child nodes. */
    virtual ApplyResult applyData(const Position& pos, int value) = 0;
};

class PredicateFactory {
public:
    virtual std::unique_ptr<Predicate> makeMultiPredicate() = 0;
};


class SinglePredicate : public Predicate {
public:
    ApplyResult applyData(const Position& pos, int value) final {
        return eval(pos) ? PRED_TRUE : PRED_FALSE;
    }

    virtual void updateStats(const Position& pos, int value) = 0;

    /** Return true if predicate is true for "pos". */
    virtual bool eval(const Position& pos) const = 0;

    /** Return true if predicate induces a non-trivial split of the data,
     *  i.e. if both stats[0] and stats[1] are non-empty. */
    virtual bool isUseful() const = 0;

    /** Entropy of data if predicate is used. */
    virtual double entropy() const = 0;
    /** Entropy of subset of data defined by predicate value. */
    virtual double entropy(bool predVal) const = 0;
    /** Entropy of data if predicate is not used. */
    virtual double entropyWithoutPred() const = 0;

    /** Return the best (lowest entropy) of this predicate and oldBest. */
    std::unique_ptr<SinglePredicate> getBest(std::unique_ptr<SinglePredicate>&& oldBest) const;

    virtual std::unique_ptr<SinglePredicate> clone() const = 0;

    virtual std::string describe() const = 0;
    virtual std::string describeChild(bool predVal) const = 0;
};


class MultiPredicate : public Predicate {
public:
    /** Return predicate corresponding to the largest information gain. */
    virtual std::unique_ptr<Predicate> getBest() const = 0;
};


/** Entropy of a distribution measured in number of bytes. */
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
