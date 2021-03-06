#ifndef WDLNODE_HPP_
#define WDLNODE_HPP_

#include "dtnode.hpp"
#include "predicates.hpp"
#include <cmath>


class WDLStatsNode;
class WDLEvalContext;


class WDLStats {
public:
    constexpr static int nWdlVals = 5;

    WDLStats() : count{} {}

    /** Return true if making a new predicate node from statsFalse/True would
     *  be better (lower cost) than keeping the current best node. */
    static bool better(const DT::Node* best, double& bestCost,
                       const WDLStats& statsFalse,
                       const WDLStats& statsTrue,
                       const DT::EvalContext& ctx);

    template <typename Pred>
    static std::unique_ptr<DT::Node> makeNode(const Pred& pred,
                                              const WDLStats& statsFalse,
                                              const WDLStats& statsTrue);

    /** Increment counter corresponding to wdlScore. */
    void incCount(int wdlScore) {
        count[wdlScore+2]++;
    }

    void applyData(int value) {
        incCount(value);
    }

    void addStats(const WDLStats& other) {
        for (int i = 0; i < nWdlVals; i++)
            count[i] += other.count[i];
    }

    void subStats(const WDLStats& other) {
        for (int i = 0; i < nWdlVals; i++)
            count[i] -= other.count[i];
    }

    bool isEmpty() const {
        for (U64 cnt : count)
            if (cnt != 0)
                return false;
        return true;
    }

    /** Return cost. (entropy or Gini impurity). */
    double cost(bool useGini) const;
    double costError(bool useGini) const;

    /** String representation of data, for debugging. */
    std::string describe(const DT::EvalContext& ctx) const;

    /** Get the i:th count. */
    U64 getCount(int i) const { return count[i]; }

    void scaleCounts(int nChunks, int appliedChunks) {
        if (nChunks != appliedChunks)
            for (int i = 0; i < nWdlVals; i++)
                count[i] = (U64)std::round((double)count[i] * nChunks / appliedChunks);
    }

private:
    /** Cost adjusted to prefer an even split when the real cost is the same. */
    double adjustedCost(bool useGini) const;

    std::array<U64,nWdlVals> count; // loss, blessed loss, draw, cursed win, win
};

class WDLStatsNode : public DT::StatsNode {
public:
    explicit WDLStatsNode(const WDLStats& stats) : stats(stats) {}

    double cost(const DT::EvalContext& ctx) const override;
    double costError(const DT::EvalContext& ctx) const;
    std::unique_ptr<DT::StatsNode> getStats(const DT::EvalContext& ctx) const override;
    std::string describe(int indentLevel, const DT::EvalContext& ctx) const override;

    void addStats(const DT::StatsNode* other) override;
    bool isEmpty() const override;
    std::unique_ptr<DT::StatsNode> mergeWithNode(const DT::StatsNode& other,
                                                 const DT::EvalContext& ctx) const override;

    std::unique_ptr<DT::EncoderNode> getEncoder() const override;
    std::unique_ptr<DT::EncoderNode> getEncoder(bool approximate) const;

    void scaleStats(int nChunks, int appliedChunks) {
        stats.scaleCounts(nChunks, appliedChunks);
    }

private:
    WDLStats stats;
};

// ------------------------------------------------------------

class CapturePredicate : public MultiPredicate {
public:
    constexpr static int minVal = -2;
    constexpr static int maxVal = 2;
    int eval(const Position& pos, DT::EvalContext& ctx) const override;
    std::string name() const override {
        return "captWdl";
    }
};

// ------------------------------------------------------------

class WDLStatsCollectorNode : public DT::StatsCollectorNode {
public:
    WDLStatsCollectorNode(const DT::EvalContext& ctx, int nChunks, double priorCost);

    bool applyData(const Position& pos, int value, DT::EvalContext& ctx) override;

    std::unique_ptr<DT::Node> getBest(const DT::EvalContext& ctx) const override;

    std::unique_ptr<DT::Node> getBestReplacement(const DT::EvalContext& ctx) const override;

private:
    template <typename Func> void iterateMembers(Func func);
    template <typename Func> void iterateMembers(Func func) const;

    /** Adjust counts based on fraction of positions sampled. */
    void reScale(std::unique_ptr<DT::Node>& node) const;

    StatsCollector<WTMPredicate, WDLStats> wtm;
    StatsCollector<InCheckPredicate, WDLStats> inCheck;
    StatsCollector<BishopPairPredicate<true>, WDLStats> bPairW;
    StatsCollector<BishopPairPredicate<false>, WDLStats> bPairB;
    StatsCollector<BishopColorPredicate<true>, WDLStats> sameB;
    StatsCollector<BishopColorPredicate<false>, WDLStats> oppoB;
    std::vector<StatsCollector<KingInPawnSquarePredicate, WDLStats>> kPawnSq;
    MultiPredStatsCollector<PawnRacePredicate, WDLStats> pRace;
    MultiPredStatsCollector<CapturePredicate, WDLStats> captWdl;
    std::vector<StatsCollector<DarkSquarePredicate, WDLStats>> darkSquare;
    std::vector<MultiPredStatsCollector<FileRankPredicate<true>, WDLStats>> fileRankF;
    std::vector<MultiPredStatsCollector<FileRankPredicate<false>, WDLStats>> fileRankR;
    std::vector<MultiPredStatsCollector<FileRankDeltaPredicate<true,false>, WDLStats>> fileDelta;
    std::vector<MultiPredStatsCollector<FileRankDeltaPredicate<false,false>, WDLStats>> rankDelta;
    std::vector<MultiPredStatsCollector<FileRankDeltaPredicate<true,true>, WDLStats>> fileDist;
    std::vector<MultiPredStatsCollector<FileRankDeltaPredicate<false,true>, WDLStats>> rankDist;
    std::vector<MultiPredStatsCollector<DistancePredicate<false>, WDLStats>> kingDist;
    std::vector<MultiPredStatsCollector<DistancePredicate<true>, WDLStats>> taxiDist;
    std::vector<StatsCollector<SameDiagPredicate, WDLStats>> diag;
    std::vector<StatsCollector<AttackPredicate, WDLStats>> attacks;
    std::vector<StatsCollector<ForkPredicate, WDLStats>> forks;
};

class WDLEncoderNode : public DT::EncoderNode {
public:
    WDLEncoderNode(const WDLStats& stats, bool approximate);

    int encodeValue(const Position& pos, int value, DT::EvalContext& ctx) const override;
    std::unique_ptr<DT::StatsNode> getStats(const DT::EvalContext& ctx) const override;
    std::string describe(int indentLevel, const DT::EvalContext& ctx) const override;

    /** Return true if "other" can encode all values "this" can encode,
     *  with the same encoding result. */
    bool subSetOf(const WDLEncoderNode& other) const;

    bool operator==(const WDLEncoderNode& other) const {
        return encTable == other.encTable;
    }

private:
    std::array<int, WDLStats::nWdlVals> encTable;
};

/** Class to compactly store WDL related information for a position. */
class WDLInfo {
public:
    int getWdl() const { return getBits(0, 3) - 2; }
    int getCaptureWdl() const { return getBits(3, 3) - 2; }
    bool getHandled() const { return getBits(7, 1); }
    U8 getData() const { return data; }

    void setWdl(int wdl) & { setBits(0, 3, wdl + 2); }
    void setCaptureWdl(int wdl) & { setBits(3, 3, wdl + 2); }
    void setHandled(bool handled) & { setBits(7, 1, handled); }
    void setData(U8 val) & { data = val; }

private:
    void setBits(int first, int size, int val) {
        int mask = ((1 << size) - 1) << first;
        data = (data & ~mask) | ((val << first) & mask);
    }

    int getBits(int first, int size) const {
        int mask = ((1 << size) - 1);
        return (data >> first) & mask;
    }

    U8 data = 0; // Bit 0-2 : wdl + 2
                 // Bit 3-5 : (best capture wdl) + 2
                 // Bit 6   : Not used
                 // Bit 7   : handled
};

class WDLUncompressedData : public DT::UncompressedData {
public:
    WDLUncompressedData(std::vector<WDLInfo>& data) : data(data) {}

    int getValue(U64 idx) const override { return data[idx].getWdl(); }
    void setEncoded(U64 idx, int value) override { data[idx].setData(value); }
    int getEncoded(U64 idx) const override { return data[idx].getData(); }

    bool isHandled(U64 idx) const override { return data[idx].getHandled(); }
    void setHandled(U64 idx, bool handled) override { data[idx].setHandled(handled); }

    int getCaptureWdl(U64 idx) const { return data[idx].getCaptureWdl(); }
    void setCaptureWdl(U64 idx, int wdl) { data[idx].setCaptureWdl(wdl); }

private:
    std::vector<WDLInfo>& data;
};

class WDLNodeFactory : public DT::NodeFactory {
public:
    explicit WDLNodeFactory(bool gini, double mergeThreshold)
    : useGiniImpurity(gini), mergeThreshold(mergeThreshold) {}

    std::unique_ptr<DT::StatsCollectorNode> makeStatsCollector(const DT::EvalContext& ctx,
                                                               int nChunks, double priorCost) override;

    std::unique_ptr<DT::EvalContext> makeEvalContext(const PosIndex& posIdx) override;

private:
    const bool useGiniImpurity;
    const double mergeThreshold;
};

class WDLEvalContext : public DT::EvalContext {
public:
    WDLEvalContext(const PosIndex& posIdx, bool gini, double mergeThreshold)
    : DT::EvalContext(posIdx), gini(gini), mergeThreshold(mergeThreshold) {}

    void init(const Position& pos, const DT::UncompressedData& data, U64 idx) override;

    int getCaptureWdl() const { return captWdl; }

    bool useGini() const { return gini; }
    double getMergeThreshold() const override { return mergeThreshold; }

private:
    int captWdl = 0;
    const bool gini;
    const double mergeThreshold;
};

template <typename Pred>
std::unique_ptr<DT::Node> WDLStats::makeNode(const Pred& pred,
                                             const WDLStats& statsFalse,
                                             const WDLStats& statsTrue) {
    if (statsFalse.isEmpty()) {
        return make_unique<WDLStatsNode>(statsTrue);
    } else if (statsTrue.isEmpty()) {
        return make_unique<WDLStatsNode>(statsFalse);
    } else {
        auto ret = make_unique<DT::PredicateNode>();
        ret->pred = make_unique<Pred>(pred);
        ret->left = make_unique<WDLStatsNode>(statsFalse);
        ret->right = make_unique<WDLStatsNode>(statsTrue);
        return std::move(ret);
    }
}

inline int
CapturePredicate::eval(const Position& pos, DT::EvalContext& ctx) const {
    const WDLEvalContext& wdlCtx = static_cast<const WDLEvalContext&>(ctx);
    return wdlCtx.getCaptureWdl();
}

#endif /* WDLNODE_HPP_ */
