#ifndef WDLPREDICATE_HPP_
#define WDLPREDICATE_HPP_

#include "predicate.hpp"
#include "moveGen.hpp"
#include <array>
#include <cmath>


class WDLPredicateFactory : public PredicateFactory {
public:
    std::unique_ptr<Predicate> makeMultiPredicate() override;
};


struct WDLStats {
    WDLStats() : count{} {}

    /** Increment counter corresponding to wdlScore. */
    void incCount(int wdlScore) {
        assert(wdlScore+2 >= 0); // FIXME!! Remove
        assert(wdlScore+2 < 5);
        count[wdlScore+2]++;
    }

    /** Entropy measured in number of bytes. */
    double entropy() const;

    /** String representation of data, for debugging. */
    std::string describe() const;

    std::array<U64,5> count; // loss, blessed loss, draw, cursed win, win
};

// ----------------------------------------------------------------------------

class WDLSinglePredicate : public SinglePredicate {
public:
    void updateStats(const Position& pos, int value) override {
        int idx = eval(pos);
        stats[idx].incCount(value);
    }

    /** Return true if predicate induces a non-trivial split of the data,
     *  i.e. if both stats[0] and stats[1] are non-empty. */
    bool isUseful() const override;

    /** Entropy of data if predicate is used. */
    double entropy() const override {
        return stats[0].entropy() + stats[1].entropy();
    }
    /** Entropy of subset of data defined by predicate value. */
    double entropy(bool predVal) const override {
        int idx = predVal;
        return stats[idx].entropy();
    }
    /** Entropy of data if predicate is not used. */
    double entropyWithoutPred() const override {
        WDLStats sum;
        for (int i = 0; i < 5; i++)
            sum.count[i] = stats[0].count[i] + stats[1].count[i];
        return sum.entropy();
    }

    std::string describe() const final;
    std::string describeChild(bool predVal) const final;

private:
    virtual std::string describePredName() const = 0;

    WDLStats stats[2]; // Statistics for true/false value of predicate
};

class WTMPredicate : public WDLSinglePredicate {
public:
    bool eval(const Position& pos) const override {
        return pos.isWhiteMove();
    }
    std::unique_ptr<SinglePredicate> clone() const override {
        return make_unique<WTMPredicate>(*this);
    }
private:
    std::string describePredName() const override {
        return "wtm";
    }
};

class InCheckPredicate : public WDLSinglePredicate {
public:
    bool eval(const Position& pos) const override {
        return MoveGen::inCheck(pos);
    }
    std::unique_ptr<SinglePredicate> clone() const override {
        return make_unique<InCheckPredicate>(*this);
    }
private:
    std::string describePredName() const override {
        return "incheck";
    }
};

template <bool white>
class BishopPairPredicate : public WDLSinglePredicate {
public:
    BishopPairPredicate() {}
    bool eval(const Position& pos) const override {
        U64 b = pos.pieceTypeBB(white ? Piece::WBISHOP : Piece::BBISHOP);
        return (b & BitBoard::maskDarkSq) &&
                (b & BitBoard::maskLightSq);
    }
    std::unique_ptr<SinglePredicate> clone() const override {
        return make_unique<BishopPairPredicate>(*this);
    }
private:
    std::string describePredName() const override {
        return white ? "bPairW" : "bPairB";
    }
};

// ----------------------------------------------------------------------------

template <int nVals>
class IntegerMultiPredicate {
public:
    /** Return the predicate value 0 <= ret < nVals corresponding to pos. */
    virtual int eval(const Position& pos) const = 0;

    void updateStats(const Position& pos, int value) {
    }

private:
    WDLStats stats[nVals];
};

// ----------------------------------------------------------------------------

/** A collection of all possible predicates. Collects statistics about
 *  how successful each predicate is at reducing the entropy of the data. */
class WDLMultiPredicate : public MultiPredicate {
public:
    ApplyResult applyData(const Position& pos, int value) override;

    /** Return predicate corresponding to the largest information gain. */
    std::unique_ptr<Predicate> getBest() const override;

private:
    WTMPredicate wtmPred;
    InCheckPredicate inCheckPred;
    BishopPairPredicate<true> bbpPredW;
    BishopPairPredicate<false> bbpPredB;
};


#endif /* WDLPREDICATED_HPP_ */
