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
    std::array<double,5> cnt;
    double tot = 0.0;
    for (int i = 0; i < 5; i++) {
        double tmp = count[i];
        cnt[i] = tmp;
        tot += tmp;
    }
    if (tot > 0) {
        for (int i = 0; i < 5; i++) {
            cnt[i] /= tot;
            cnt[i] = std::min(floor(cnt[i] * 100), 99.0);
        }
    }

    std::stringstream ss;
    ss << std::scientific << std::setprecision(2) << tot;
    ss << " [";
    for (int i = 0; i < 5; i++) {
        if (i > 0) ss << ' ';
//        ss << std::setw(2) << std::setfill('0') << (int)cnt[i];
        ss << count[i];
    }
    ss << "] " << std::setfill(' ');

    std::vector<std::pair<U64,int>> srt;
    for (int i = 0; i < 5; i++)
        srt.emplace_back(~0ULL - count[i], i);
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < 5; i++)
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
    // FIXME!!
    return nullptr;
}

// ------------------------------------------------------------

bool
WDLStatsCollectorNode::applyData(const Position& pos, int value) {
    wtm.applyData(pos, value);
    inCheck.applyData(pos, value);
    bPairW.applyData(pos, value);
    bPairB.applyData(pos, value);
    return true;
}

std::unique_ptr<DT::Node>
WDLStatsCollectorNode::getBest() const {
    std::unique_ptr<DT::Node> best;
    best = wtm.getBest(std::move(best));
    best = inCheck.getBest(std::move(best));
    best = bPairW.getBest(std::move(best));
    best = bPairB.getBest(std::move(best));
    return best;
}

std::unique_ptr<DT::StatsNode>
WDLEncoderNode::getStats() const {
    return make_unique<WDLStatsNode>(WDLStats{});
}
