#ifndef WDLNODE_HPP_
#define WDLNODE_HPP_

#include "dtnode.hpp"
#include "predicates.hpp"


class WDLNodeFactory : public DT::NodeFactory {
public:
    std::unique_ptr<DT::StatsCollectorNode> makeStatsCollector() override;
};

class WDLStatsNode;


struct WDLStats {
    constexpr static int nWdlVals = 5;

    WDLStats() : count{} {}

    template <typename Pred>
    static std::unique_ptr<DT::Node> makeNode(const Pred& pred, const WDLStats& stats1,
                                              const WDLStats& stats2) {
        if (stats1.isEmpty()) {
            return make_unique<WDLStatsNode>(stats2);
        } else if (stats2.isEmpty()) {
            return make_unique<WDLStatsNode>(stats1);
        } else {
            auto ret = make_unique<DT::PredicateNode>();
            ret->pred = make_unique<Pred>(pred);
            ret->left = make_unique<WDLStatsNode>(stats1);
            ret->right = make_unique<WDLStatsNode>(stats2);
            return std::move(ret);
        }
    }

    /** Increment counter corresponding to wdlScore. */
    void incCount(int wdlScore) {
        assert(wdlScore+2 >= 0);                // FIXME!! Remove
        assert(wdlScore+2 < nWdlVals);
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

    /** Entropy measured in number of bytes. */
    double entropy() const;

    /** String representation of data, for debugging. */
    std::string describe() const;

    std::array<U64,nWdlVals> count; // loss, blessed loss, draw, cursed win, win
};

class WDLStatsNode : public DT::StatsNode {
public:
    explicit WDLStatsNode(const WDLStats& stats) : stats(stats) {}

    double entropy() const override;
    std::unique_ptr<DT::StatsNode> getStats() const override;
    std::string describe(int indentLevel) const override;

    void addStats(const DT::StatsNode* other) override;
    bool isEmpty() const override;
    std::unique_ptr<DT::StatsNode> mergeWithNode(const DT::StatsNode& other) const override;

    std::unique_ptr<DT::EncoderNode> getEncoder() const override;

    WDLStats stats;
};

class WDLStatsCollectorNode : public DT::StatsCollectorNode {
public:
    bool applyData(const Position& pos, int value) override;

    std::unique_ptr<DT::Node> getBest() const override;

    StatsCollector<WTMPredicate, WDLStats> wtm;
    StatsCollector<InCheckPredicate, WDLStats> inCheck;
    StatsCollector<BishopPairPredicate<true>, WDLStats> bPairW;
    StatsCollector<BishopPairPredicate<false>, WDLStats> bPairB;
    StatsCollector<BishopColorPredicate<true>, WDLStats> sameB;
    StatsCollector<BishopColorPredicate<false>, WDLStats> oppoB;
    MultiPredStatsCollector<PawnRacePredicate, WDLStats> pRace;
};

class WDLEncoderNode : public DT::EncoderNode {
public:
    WDLEncoderNode(const WDLStats& stats);

    int encodeValue(const Position& pos, int value) const override;
    std::unique_ptr<DT::StatsNode> getStats() const override;
    std::string describe(int indentLevel) const override;

    /** Return true if "other" can encode all values "this" can encode,
     *  with the same encoding result. */
    bool subSetOf(const WDLEncoderNode& other) const;

    /** Return true if this node corresponds to non-zero entropy. */
    bool hasEntropy() const;

    bool operator==(const WDLEncoderNode& other) const {
        return encTable == other.encTable;
    }

    std::array<int, WDLStats::nWdlVals> encTable;
};


#endif /* WDLNODE_HPP_ */
