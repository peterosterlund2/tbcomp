#include "wdlnode.hpp"
#include "textio.hpp"
#include <numeric>
#include <cmath>


bool
WDLStats::better(const DT::Node* best,
                 const WDLStats& statsFalse,
                 const WDLStats& statsTrue) {
    if (!best)
        return true;
    auto bestP = dynamic_cast<const DT::PredicateNode*>(best);
    if (!bestP)
        return true;
    auto childL = dynamic_cast<const WDLStatsNode*>(bestP->left.get());
    auto childR = dynamic_cast<const WDLStatsNode*>(bestP->right.get());
    if (!childL || !childR)
        return true;

    double currEntropy = childL->stats.adjustedEntropy() +
                         childR->stats.adjustedEntropy();
    double newEntropy = statsFalse.adjustedEntropy() +
                        statsTrue.adjustedEntropy();

    return currEntropy > newEntropy;
}

double
WDLStats::entropy() const {
    return ::entropy(count.begin(), count.end());
}

double
WDLStats::adjustedEntropy() const {
    double sum = std::accumulate(count.begin(), count.end(), 0);
    double bits = sum > 0 ? std::log2(sum) : 0;
    return entropy() + (64 - bits) * 1e-4;
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

std::unique_ptr<DT::StatsNode>
WDLStatsNode::mergeWithNode(const DT::StatsNode& other) const {
    const WDLStatsNode& otherWdl = static_cast<const WDLStatsNode&>(other);
    bool merge = false;

    WDLStats sum(stats);
    sum.addStats(otherWdl.stats);
    double entrDiff = sum.entropy() - (stats.entropy() + otherWdl.stats.entropy());
    if (entrDiff <= 8)
        merge = true;

    if (!merge) {
        auto enc1 = getEncoder();
        auto enc2 = otherWdl.getEncoder();
        const WDLEncoderNode& wdlEnc1 = static_cast<const WDLEncoderNode&>(*enc1.get());
        const WDLEncoderNode& wdlEnc2 = static_cast<const WDLEncoderNode&>(*enc2.get());
        if (wdlEnc1 == wdlEnc2) {
            merge = true;
        } else if ((wdlEnc1.subSetOf(wdlEnc2) && wdlEnc1.hasEntropy()) ||
                   (wdlEnc2.subSetOf(wdlEnc1) && wdlEnc2.hasEntropy())) {
            merge = true;
        }
    }

    if (!merge)
        return nullptr;

    return make_unique<WDLStatsNode>(sum);
}

std::unique_ptr<DT::EncoderNode>
WDLStatsNode::getEncoder() const {
    return make_unique<WDLEncoderNode>(stats);
}

// ------------------------------------------------------------

WDLStatsCollectorNode::WDLStatsCollectorNode(DT::EvalContext& ctx) {
    int nPieces = ctx.numPieces();
    for (int i = 0; i < nPieces; i++)
        if (Piece::makeWhite(ctx.getPieceType(i)) == Piece::WPAWN)
            kPawnSq.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        darkSquare.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        fileRankW.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        fileRankB.emplace_back(i);
    for (int p1 = 0; p1 < nPieces; p1++) {
        for (int p2 = p1+1; p2 < nPieces; p2++) {
            fileDelta.emplace_back(p1,p2);
            rankDelta.emplace_back(p1,p2);
            fileDist.emplace_back(p1,p2);
            rankDist.emplace_back(p1,p2);
        }
    }
}

bool
WDLStatsCollectorNode::applyData(const Position& pos, int value, DT::EvalContext& ctx) {
    wtm.applyData(pos, ctx, value);
    inCheck.applyData(pos, ctx, value);
    bPairW.applyData(pos, ctx, value);
    bPairB.applyData(pos, ctx, value);
    sameB.applyData(pos, ctx, value);
    oppoB.applyData(pos, ctx, value);
    for (auto& p : kPawnSq)
        p.applyData(pos, ctx, value);
    pRace.applyData(pos, ctx, value);
    captWdl.applyData(pos, ctx, value);
    for (auto& p : darkSquare)
        p.applyData(pos, ctx, value);
    for (auto& p : fileRankW)
        p.applyData(pos, ctx, value);
    for (auto& p : fileRankB)
        p.applyData(pos, ctx, value);
    for (auto& p : fileDelta)
        p.applyData(pos, ctx, value);
    for (auto& p : rankDelta)
        p.applyData(pos, ctx, value);
    for (auto& p : fileDist)
        p.applyData(pos, ctx, value);
    for (auto& p : rankDist)
        p.applyData(pos, ctx, value);
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
    for (auto& p : kPawnSq)
        p.updateBest(best);
    pRace.updateBest(best);
    captWdl.updateBest(best);
    for (auto& p : darkSquare)
        p.updateBest(best);
    for (auto& p : fileRankW)
        p.updateBest(best);
    for (auto& p : fileRankB)
        p.updateBest(best);
    for (auto& p : fileDelta)
        p.updateBest(best);
    for (auto& p : rankDelta)
        p.updateBest(best);
    for (auto& p : fileDist)
        p.updateBest(best);
    for (auto& p : rankDist)
        p.updateBest(best);
    return best;
}

// ------------------------------------------------------------

WDLEncoderNode::WDLEncoderNode(const WDLStats& stats) {
    constexpr int N = WDLStats::nWdlVals;
    std::array<std::pair<U64,int>,N> srt;
    const U64 maxVal = ~0ULL;
    for (int i = 0; i < N; i++)
        srt[i] = std::make_pair(maxVal - stats.count[i], stats.count[i] ? i : -1);
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < N; i++)
        encTable[i] = srt[i].second;
}

int
WDLEncoderNode::encodeValue(const Position& pos, int value, DT::EvalContext& ctx) const {
    const WDLEvalContext& wdlCtx = static_cast<const WDLEvalContext&>(ctx);
    constexpr int N = WDLStats::nWdlVals;
    int ret = 0;
    for (int i = 0; i < N; i++) {
        int enc = encTable[i] - 2;
        if (enc == value) {
            return ret;
        } else if (enc != -1) {
            int captWdl = wdlCtx.getCaptureWdl();
            if (pos.isWhiteMove() ? (enc >= captWdl) : (enc <= captWdl))
                ret++;
        }
    }
    assert(false);
    return ret;
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

bool
WDLEncoderNode::subSetOf(const WDLEncoderNode& other) const {
    for (int i = 0; i < WDLStats::nWdlVals && (encTable[i] != -1); i++)
        if (encTable[i] != other.encTable[i])
            return false;
    return true;
}

bool
WDLEncoderNode::hasEntropy() const {
    int nValid = 0;
    for (int v : encTable)
        if (v >= 0)
            nValid++;
    return nValid > 1;
}

// ------------------------------------------------------------

std::unique_ptr<DT::StatsCollectorNode>
WDLNodeFactory::makeStatsCollector(DT::EvalContext& ctx) {
    return make_unique<WDLStatsCollectorNode>(ctx);
};

std::unique_ptr<DT::EvalContext>
WDLNodeFactory::makeEvalContext(const PosIndex& posIdx) {
    return make_unique<WDLEvalContext>(posIdx);
}

// ------------------------------------------------------------

void
WDLEvalContext::init(const Position& pos,
                     const DT::UncompressedData& data, U64 idx) {
    const WDLUncompressedData& wdlData = static_cast<const WDLUncompressedData&>(data);
    captWdl = wdlData.getCaptureWdl(idx);
}
