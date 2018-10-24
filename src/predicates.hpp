#ifndef PREDICATES_HPP_
#define PREDICATES_HPP_

#include "predicate.hpp"
#include "dtnode.hpp"
#include "moveGen.hpp"
#include "util/util.hpp"
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
private:
    Pred pred;
    Stats stats[2];
};

// ----------------------------------------------------------------------------

class MultiPredicate {
public:
    virtual ~MultiPredicate() = default;

    /** Return predicate value for "pos". */
    virtual int eval(const Position& pos) const = 0;

    /** For debugging. */
    virtual std::string name() const = 0;
};

class PawnRacePredicate : public MultiPredicate {
public:
    constexpr static int minVal = -5;
    constexpr static int maxVal = 5;

    int eval(const Position& pos) const override {
        U64 wPawnMask = pos.pieceTypeBB(Piece::WPAWN);
        U64 bPawnMask = pos.pieceTypeBB(Piece::BPAWN);
        int wRank = wPawnMask == 0 ? 1 : Square::getY(BitBoard::extractSquare(wPawnMask));
        int bRank = 6;
        while (bPawnMask != 0)
            bRank = Square::getY(BitBoard::extractSquare(bPawnMask));
        int score = wRank + bRank - 7;
        return score;
    }

    std::string name() const override {
        return "pRace";
    }
};

// ----------------------------------------------------------------------------

template <typename MultiPred>
class MultiPredBound : public Predicate {
public:
    MultiPredBound(const MultiPred& pred, int lim) : pred(pred), limit(lim) {}
    bool eval(const Position& pos) const override {
        return pred.eval(pos) <= limit;
    }
    std::string name() const override {
        return pred.name() + "<=" + num2Str(limit);
    }
private:
    MultiPred pred;
    int limit;
};

template <typename MultiPred, typename Stats>
class MultiPredStatsCollector {
    constexpr static int minVal = MultiPred::minVal;
    constexpr static int maxVal = MultiPred::maxVal;
public:
    void applyData(const Position& pos, int value) {
        int idx = pred.eval(pos) - minVal;
        stats[idx].applyData(value);
    }

    void updateBest(std::unique_ptr<DT::Node>& best) const {
        const int N = maxVal - minVal + 1;
        Stats stats1, stats2;
        for (int i = 0; i < N; i++)
            stats2.addStats(stats[i]);
        for (int i = 0; i < N-1; i++) {
            stats1.addStats(stats[i]);
            stats2.subStats(stats[i]);
            if (!best || best->entropy() > stats1.entropy() + stats2.entropy())
                best = Stats::makeNode(MultiPredBound<MultiPred>(pred, minVal + i),
                                       stats1, stats2);
        }
    }
private:
    MultiPred pred;
    Stats stats[maxVal - minVal + 1];
};


#endif /* PREDICATES_HPP_ */
