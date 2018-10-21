#ifndef PREDICATES_HPP_
#define PREDICATES_HPP_

#include "predicate.hpp"
#include "dtnode.hpp"
#include "moveGen.hpp"
#include <array>


class WTMPredicate : public Predicate {
public:
    bool eval(const Position& pos) const override {
        return pos.isWhiteMove();
    }
    std::string name() const override {
        return "wtm";
    }
};

class InCheckPredicate : public Predicate {
public:
    bool eval(const Position& pos) const override {
        return MoveGen::inCheck(pos);
    }
    std::string name() const override {
        return "incheck";
    }
};

template <bool white>
class BishopPairPredicate : public Predicate {
public:
    BishopPairPredicate() {}
    bool eval(const Position& pos) const override {
        U64 b = pos.pieceTypeBB(white ? Piece::WBISHOP : Piece::BBISHOP);
        return (b & BitBoard::maskDarkSq) &&
                (b & BitBoard::maskLightSq);
    }
    std::string name() const override {
        return white ? "bPairW" : "bPairB";
    }
};

// ----------------------------------------------------------------------------

template <typename Pred, typename Stats>
class StatsCollector {
public:
    void applyData(const Position& pos, int value) {
        stats[pred.eval(pos)].applyData(value);
    }

    /** Return the best (lowest entropy) of this node and oldBest. */
    std::unique_ptr<DT::Node> getBest(std::unique_ptr<DT::Node>&& oldBest) const {
        if (oldBest && oldBest->entropy() <= stats[0].entropy() + stats[1].entropy())
            return std::move(oldBest);
        return Stats::makeNode(pred, stats[0], stats[1]);
    }

    Pred pred;
    Stats stats[2];
};


template <typename Pred, typename Stats, int nVals>
class MultiPredStatsCollector {
public:
    /** Return the predicate value 0 <= ret < nVals corresponding to pos. */
//    virtual int eval(const Position& pos) const = 0;

    void applyData(const Position& pos, int value) {
        stats[pred.eval(pos, value)].applyData(value);
    }

private:
    Pred pred;
    Stats stats[nVals];
};


#endif /* PREDICATES_HPP_ */
