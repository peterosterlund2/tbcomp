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

/** Test if white/black have bishops on the same/opposite colors. */
template <bool sameColor>
class BishopColorPredicate : public Predicate {
public:
    BishopColorPredicate() {}
    bool eval(const Position& pos) const override {
        U64 wb = pos.pieceTypeBB(Piece::WBISHOP);
        U64 bb = pos.pieceTypeBB(Piece::BBISHOP);
        U64 d = BitBoard::maskDarkSq;
        U64 l = BitBoard::maskLightSq;
        if (sameColor) {
            return ((wb & d) && (bb & d)) ||
                   ((wb & l) && (bb & l));
        } else {
            return ((wb & d) && (bb & l)) ||
                   ((wb & l) && (bb & d));
        }
    }
    std::string name() const override {
        return sameColor ? "sameB" : "oppoB";
    }
};

// ----------------------------------------------------------------------------

template <typename Pred, typename Stats>
class StatsCollector {
public:
    void applyData(const Position& pos, int value) {
        stats[pred.eval(pos)].applyData(value);
    }

    /** Update best if this node has lower entropy. */
    void updateBest(std::unique_ptr<DT::Node>& best) const {
        if (!best || best->entropy() > stats[0].entropy() + stats[1].entropy())
            best = Stats::makeNode(pred, stats[0], stats[1]);
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
