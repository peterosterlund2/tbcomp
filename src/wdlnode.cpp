#include "wdlnode.hpp"


std::unique_ptr<DT::StatsCollectorNode>
WDLNodeFactory::makeStatsCollector() {
    return make_unique<WDLStatsCollectorNode>();
};

// ------------------------------------------------------------

double
WDLStats::entropy() const {
    return ::entropy(count.begin(), count.end());
}

std::string
WDLStats::describe() const {
    constexpr int N = nWdlVals;
    std::array<double,N> cnt;
    double tot = 0.0;
    for (int i = 0; i < N; i++) {
        double tmp = count[i];
        cnt[i] = tmp;
        tot += tmp;
    }
    if (tot > 0) {
        for (int i = 0; i < N; i++) {
            cnt[i] /= tot;
            cnt[i] = std::min(floor(cnt[i] * 100), 99.0);
        }
    }

    std::stringstream ss;
    ss << std::scientific << std::setprecision(2) << tot;
    ss << " [";
    for (int i = 0; i < N; i++) {
        if (i > 0) ss << ' ';
//        ss << std::setw(2) << std::setfill('0') << (int)cnt[i];
        ss << count[i];
    }
    ss << "] " << std::setfill(' ');

    std::vector<std::pair<U64,int>> srt;
    srt.reserve(N);
    for (int i = 0; i < N; i++)
        srt.emplace_back(~0ULL - count[i], i);
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < N; i++)
        ss << srt[i].second;

    ss << ' ' << entropy();

    return ss.str();
}

// ------------------------------------------------------------

double
WDLStatsNode::entropy() const {
    return stats.entropy();
}

std::unique_ptr<DT::StatsNode>
WDLStatsNode::getStats() const {
    return make_unique<WDLStatsNode>(*this);
}

std::string
WDLStatsNode::describe(int indentLevel) const {
    std::stringstream ss;
    ss << std::string(indentLevel*2, ' ') << stats.describe() << '\n';
    return ss.str();
}

void
WDLStatsNode::addStats(const DT::StatsNode* other) {
    stats.addStats(static_cast<const WDLStatsNode&>(*other).stats);
}

bool
WDLStatsNode::isEmpty() const {
    return stats.isEmpty();
}

std::unique_ptr<DT::EncoderNode>
WDLStatsNode::getEncoder() {
    return make_unique<WDLEncoderNode>(stats);
}

// ------------------------------------------------------------

bool
WDLStatsCollectorNode::applyData(const Position& pos, int value) {
    wtm.applyData(pos, value);
    inCheck.applyData(pos, value);
    bPairW.applyData(pos, value);
    bPairB.applyData(pos, value);
    sameB.applyData(pos, value);
    oppoB.applyData(pos, value);
    return true;
}

std::unique_ptr<DT::Node>
WDLStatsCollectorNode::getBest() const {
    std::unique_ptr<DT::Node> best;
    wtm.updateBest(best);
    inCheck.updateBest(best);
    bPairW.updateBest(best);
    bPairB.updateBest(best);
    sameB.updateBest(best);
    oppoB.updateBest(best);
    return best;
}

// ------------------------------------------------------------

WDLEncoderNode::WDLEncoderNode(const WDLStats& stats) {
    constexpr int N = WDLStats::nWdlVals;
    std::array<std::pair<U64,int>,N> srt;
    const U64 maxVal = ~0ULL;
    for (int i = 0; i < N; i++)
        srt[i] = std::make_pair(maxVal - stats.count[i], i);
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < N; i++)
        encTable[srt[i].second] = srt[i].first == maxVal ? -1 : i;
}

int
WDLEncoderNode::encodeValue(const Position& pos, int value) const {
    return encTable[value+2];
}

std::unique_ptr<DT::StatsNode>
WDLEncoderNode::getStats() const {
    return make_unique<WDLStatsNode>(WDLStats{});
}

std::string
WDLEncoderNode::describe(int indentLevel) const {
    std::stringstream ss;
    ss << std::string(indentLevel*2, ' ');
    for (int v : encTable) {
        if (v == -1)
            ss << '.';
        else
            ss << v;
    }
    ss << '\n';
    return ss.str();
}
