#ifndef PREDICATES_HPP_
#define PREDICATES_HPP_

#include "predicate.hpp"
#include "dtnode.hpp"
#include "moveGen.hpp"
#include "util/util.hpp"
#include <array>


class WTMPredicate : public Predicate {
public:
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        return pos.isWhiteMove();
    }
    std::string name() const override {
        return "wtm";
    }
};

class InCheckPredicate : public Predicate {
public:
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        return MoveGen::inCheck(pos);
    }
    std::string name() const override {
        return "incheck";
    }
};

template <bool white>
class BishopPairPredicate : public Predicate {
public:
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
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
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
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

class DarkSquarePredicate : public Predicate {
public:
    explicit DarkSquarePredicate(int pieceNo) : pieceNo(pieceNo) {}
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq = ctx.getPieceSquare(pieceNo, pos);
        return BitBoard::maskDarkSq & (1ULL << sq);
    }
    std::string name() const override {
        return "darkSq" + num2Str(pieceNo);
    }
private:
    int pieceNo;
};

/** Predicate is true if opponent king is within "the square" of a pawn. */
class KingPawnSquarePredicate : public Predicate {
public:
    explicit KingPawnSquarePredicate(int pieceNo) : pieceNo(pieceNo) {}
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        int pSq = ctx.getPieceSquare(pieceNo, pos);
        int x = Square::getX(pSq);
        int y = Square::getY(pSq);
        switch (ctx.getPieceType(pieceNo)) {
        case Piece::WPAWN: {
            int kSq = pos.getKingSq(false);
            int pawnDist = std::min(5, 7 - y);
            int kingDist = BitBoard::getKingDistance(kSq, Square::getSquare(x, 7));
            if (!pos.isWhiteMove())
                kingDist--;
            return kingDist <= pawnDist;
        }
        case Piece::BPAWN: {
            int kSq = pos.getKingSq(true);
            int pawnDist = std::min(5, y);
            int kingDist = BitBoard::getKingDistance(kSq, Square::getSquare(x, 0));
            if (pos.isWhiteMove())
                kingDist--;
            return kingDist <= pawnDist;
        }
        default:
            return false;
        }
    }
    std::string name() const override {
        return "kPawnSq" + num2Str(pieceNo);
    }
private:
    int pieceNo;
};

/** Predicate true if piece p1 attacks piece p2. */
class AttackPredicate : public Predicate {
public:
    AttackPredicate(int p1, int p2) : p1(p1), p2(p2) {}
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq1 = ctx.getPieceSquare(p1, pos);
        U64 sq2Mask = 1ULL << ctx.getPieceSquare(p2, pos);
        switch (ctx.getPieceType(p1)) {
        case Piece::WKING: case Piece::BKING:
            return BitBoard::kingAttacks(sq1) & sq2Mask;
        case Piece::WQUEEN: case Piece::BQUEEN:
            if (BitBoard::bishopAttacks(sq1, pos.occupiedBB()) & sq2Mask)
                return true;
            // Fall through
        case Piece::WROOK: case Piece::BROOK:
            return BitBoard::rookAttacks(sq1, pos.occupiedBB()) & sq2Mask;
        case Piece::WBISHOP: case Piece::BBISHOP:
            return BitBoard::bishopAttacks(sq1, pos.occupiedBB()) & sq2Mask;
        case Piece::WKNIGHT: case Piece::BKNIGHT:
            return BitBoard::knightAttacks(sq1) & sq2Mask;
        case Piece::WPAWN:
            return BitBoard::wPawnAttacks(sq1) & sq2Mask;
        case Piece::BPAWN:
            return BitBoard::bPawnAttacks(sq1) & sq2Mask;
        default:
            return false;
        }
    }
    std::string name() const override {
        return "attack" + num2Str(p1) + num2Str(p2);
    }
private:
    int p1;
    int p2;
};

/** Predicate true if piece p1 and piece p2 on the same diagonal. */
class DiagonalPredicate : public Predicate {
public:
    DiagonalPredicate(int p1, int p2) : p1(p1), p2(p2) {}
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq1 = ctx.getPieceSquare(p1, pos);
        int sq2 = ctx.getPieceSquare(p2, pos);
        return BitBoard::bishopAttacks(sq1, 0) & (1ULL << sq2);
    }
    std::string name() const override {
        return "diag" + num2Str(p1) + num2Str(p2);
    }
private:
    int p1;
    int p2;
};

/** Predicate true if pieces p1 and p2 can be knight forked by the opponent. */
class ForkPredicate : public Predicate {
public:
    ForkPredicate(int p1, int p2, const DT::EvalContext& ctx) : p1(p1), p2(p2) {
        forker = Piece::isWhite(ctx.getPieceType(p1)) ? Piece::BKNIGHT : Piece::WKNIGHT;
    }
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq1 = ctx.getPieceSquare(p1, pos);
        int sq2 = ctx.getPieceSquare(p2, pos);
        U64 atk = 0;
        U64 m = pos.pieceTypeBB(forker);
        while (m) {
            int sq = BitBoard::extractSquare(m);
            atk |= BitBoard::knightAttacks(sq);
        }
        atk &= BitBoard::knightAttacks(sq1);
        atk &= BitBoard::knightAttacks(sq2);
        return atk != 0;
    }
    std::string name() const override {
        return "fork" + num2Str(p1) + num2Str(p2);
    }
private:
    int p1;
    int p2;
    Piece::Type forker;
};

// ----------------------------------------------------------------------------

template <typename Pred, typename Stats>
class StatsCollector {
public:
    template <typename... Args>
    explicit StatsCollector(Args&&... args) : pred(std::forward<Args>(args)...) {}

    void applyData(const Position& pos, DT::EvalContext& ctx, int value) {
        stats[pred.eval(pos,ctx)].applyData(value);
    }

    /** Update best if this node has lower cost. */
    void updateBest(std::unique_ptr<DT::Node>& best, double& bestCost) const {
        if (Stats::better(best.get(), bestCost, stats[0], stats[1]))
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
    virtual int eval(const Position& pos, DT::EvalContext& ctx) const = 0;

    /** For debugging. */
    virtual std::string name() const = 0;
};

class PawnRacePredicate : public MultiPredicate {
public:
    constexpr static int minVal = -5;
    constexpr static int maxVal = 5;

    int eval(const Position& pos, DT::EvalContext& ctx) const override {
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

template <bool file>
class FileRankPredicate : public MultiPredicate {
public:
    constexpr static int minVal = 0;
    constexpr static int maxVal = 7;
    explicit FileRankPredicate(int pieceNo) : pieceNo(pieceNo) {}
    int eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq = ctx.getPieceSquare(pieceNo, pos);
        return file ? Square::getX(sq) : Square::getY(sq);
    }
    std::string name() const override {
        return (file ? "file" : "rank") + num2Str(pieceNo);
    }
private:
    int pieceNo;
};

template <bool file, bool absVal>
class FileRankDeltaPredicate : public MultiPredicate {
public:
    constexpr static int minVal = absVal ? 0 : -7;
    constexpr static int maxVal = 7;
    FileRankDeltaPredicate(int p1, int p2) : p1(p1), p2(p2) {}
    int eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq1 = ctx.getPieceSquare(p1, pos);
        int sq2 = ctx.getPieceSquare(p2, pos);
        int d = file ? Square::getX(sq2) - Square::getX(sq1)
                     : Square::getY(sq2) - Square::getY(sq1);
        if (absVal)
            d = std::abs(d);
        return d;
    }
    std::string name() const override {
        return std::string(file ? "file" : "rank") + (absVal ? "Dist" : "Delta") +
               num2Str(p1) + num2Str(p2);
    }
private:
    int p1;
    int p2;
};

template <bool taxi>
class DistancePredicate : public MultiPredicate {
public:
    constexpr static int minVal = 1;
    constexpr static int maxVal = taxi ? 14 : 7;
    DistancePredicate(int p1, int p2) : p1(p1), p2(p2) {}
    int eval(const Position& pos, DT::EvalContext& ctx) const override {
        int sq1 = ctx.getPieceSquare(p1, pos);
        int sq2 = ctx.getPieceSquare(p2, pos);
        return taxi ? BitBoard::getTaxiDistance(sq1, sq2)
                    : BitBoard::getKingDistance(sq1, sq2);
    }
    std::string name() const override {
        return std::string(taxi ? "taxi" : "dist") + num2Str(p1) + num2Str(p2);
    }
private:
    int p1;
    int p2;
};

// ----------------------------------------------------------------------------

template <typename MultiPred>
class MultiPredBound : public Predicate {
public:
    MultiPredBound(const MultiPred& pred, int lim) : pred(pred), limit(lim) {}
    bool eval(const Position& pos, DT::EvalContext& ctx) const override {
        return pred.eval(pos, ctx) <= limit;
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
    template <typename... Args>
    explicit MultiPredStatsCollector(Args&&... args) : pred(std::forward<Args>(args)...) {}

    void applyData(const Position& pos, DT::EvalContext& ctx, int value) {
        int idx = pred.eval(pos, ctx) - minVal;
        stats[idx].applyData(value);
    }

    void updateBest(std::unique_ptr<DT::Node>& best, double& bestCost) const {
        const int N = maxVal - minVal + 1;
        Stats statsTrue, statsFalse;
        for (int i = 0; i < N; i++)
            statsFalse.addStats(stats[i]);
        for (int i = 0; i < N-1; i++) {
            statsTrue.addStats(stats[i]);
            statsFalse.subStats(stats[i]);
            if (Stats::better(best.get(), bestCost, statsFalse, statsTrue))
                best = Stats::makeNode(MultiPredBound<MultiPred>(pred, minVal + i),
                                       statsFalse, statsTrue);
        }
    }
private:
    MultiPred pred;
    Stats stats[maxVal - minVal + 1];
};


#endif /* PREDICATES_HPP_ */
