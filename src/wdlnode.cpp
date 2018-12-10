#include "wdlnode.hpp"
#include "tbutil.hpp"
#include "textio.hpp"
#include <numeric>
#include <cfloat>


bool
WDLStats::better(const DT::Node* best, double& bestCost,
                 const WDLStats& statsFalse,
                 const WDLStats& statsTrue,
                 const DT::EvalContext& ctx) {
    bool useGini = static_cast<const WDLEvalContext&>(ctx).useGini();
    double newCost = statsFalse.adjustedCost(useGini) +
                     statsTrue.adjustedCost(useGini);
    if (newCost < bestCost) {
        bestCost = newCost;
        return true;
    }
    return best == nullptr;
}

double
WDLStats::cost(bool useGini) const {
    if (useGini) {
        return ::giniImpurity(count.begin(), count.end());
    } else {
        return ::entropy(count.begin(), count.end());
    }
}

double
WDLStats::adjustedCost(bool useGini) const {
    U64 sum = std::accumulate(count.begin(), count.end(), (U64)0);
    int bits = (sum >= (1ULL<<32)) ? floorLog2((U32)(sum >> 32)) + 32 : floorLog2((U32)sum);
    return cost(useGini) + (64 - bits) * 1e-4;
}

std::string
WDLStats::describe(const DT::EvalContext& ctx) const {
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

    bool useGini = static_cast<const WDLEvalContext&>(ctx).useGini();
    ss << ' ' << cost(useGini);

    return ss.str();
}

// ------------------------------------------------------------

double
WDLStatsNode::cost(const DT::EvalContext& ctx) const {
    bool useGini = static_cast<const WDLEvalContext&>(ctx).useGini();
    return stats.cost(useGini);
}

std::unique_ptr<DT::StatsNode>
WDLStatsNode::getStats(const DT::EvalContext& ctx) const {
    return make_unique<WDLStatsNode>(*this);
}

std::string
WDLStatsNode::describe(int indentLevel, const DT::EvalContext& ctx) const {
    std::stringstream ss;
    ss << std::string(indentLevel*2, ' ') << stats.describe(ctx) << '\n';
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
WDLStatsNode::mergeWithNode(const DT::StatsNode& other, const DT::EvalContext& ctx) const {
    const WDLStatsNode& otherWdl = static_cast<const WDLStatsNode&>(other);
    bool merge = false;

    WDLStats sum(stats);
    sum.addStats(otherWdl.stats);
    const auto& wdlCtx = static_cast<const WDLEvalContext&>(ctx);
    bool useGini = false;
    double costDiff = sum.cost(useGini) - (stats.cost(useGini) + otherWdl.stats.cost(useGini));
    if (costDiff <= wdlCtx.getMergeThreshold())
        merge = true;

    if (!merge) {
        auto enc1 = getEncoder(false);
        auto enc2 = otherWdl.getEncoder(false);
        const WDLEncoderNode& wdlEnc1 = static_cast<const WDLEncoderNode&>(*enc1.get());
        const WDLEncoderNode& wdlEnc2 = static_cast<const WDLEncoderNode&>(*enc2.get());
        if (wdlEnc1 == wdlEnc2) {
            merge = true;
        } else if (wdlEnc1.subSetOf(wdlEnc2) || wdlEnc2.subSetOf(wdlEnc1)) {
            merge = true;
        }
    }

    if (!merge)
        return nullptr;

    return make_unique<WDLStatsNode>(sum);
}

std::unique_ptr<DT::EncoderNode>
WDLStatsNode::getEncoder() const {
    return make_unique<WDLEncoderNode>(stats, true);
}

std::unique_ptr<DT::EncoderNode>
WDLStatsNode::getEncoder(bool approximate) const {
    return make_unique<WDLEncoderNode>(stats, approximate);
}

// ------------------------------------------------------------

WDLStatsCollectorNode::WDLStatsCollectorNode(DT::EvalContext& ctx, int nChunks)
    : StatsCollectorNode(nChunks) {
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
            fileDelta.emplace_back(p1, p2);
            rankDelta.emplace_back(p1, p2);
            fileDist.emplace_back(p1, p2);
            rankDist.emplace_back(p1, p2);
            kingDist.emplace_back(p1, p2);
            taxiDist.emplace_back(p1, p2);
            diag.emplace_back(p1, p2);

            bool white1 = Piece::isWhite(ctx.getPieceType(p1));
            bool white2 = Piece::isWhite(ctx.getPieceType(p2));
            if (white1 == white2) {
                for (int p3 = 0; p3 < nPieces; p3++) {
                    Piece::Type pt = ctx.getPieceType(p3);
                    if ((Piece::isWhite(pt) != white1) &&
                        (Piece::makeWhite(pt) == Piece::WKNIGHT)) {
                        forks.emplace_back(p1, p2, ctx);
                        break;
                    }
                }
            }
        }
    }
    for (int p1 = 0; p1 < nPieces; p1++)
        for (int p2 = 0; p2 < nPieces; p2++)
            if (p1 != p2)
                attacks.emplace_back(p1, p2);
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
    for (auto& p : kingDist)
        p.applyData(pos, ctx, value);
    for (auto& p : taxiDist)
        p.applyData(pos, ctx, value);
    for (auto& p : diag)
        p.applyData(pos, ctx, value);
    for (auto& p : forks)
        p.applyData(pos, ctx, value);
    for (auto& p : attacks)
        p.applyData(pos, ctx, value);
    return true;
}

std::unique_ptr<DT::Node>
WDLStatsCollectorNode::getBest(const DT::EvalContext& ctx) const {
    std::unique_ptr<DT::Node> best;
    double bestCost = DBL_MAX;
    wtm.updateBest(best, bestCost, ctx);
    inCheck.updateBest(best, bestCost, ctx);
    bPairW.updateBest(best, bestCost, ctx);
    bPairB.updateBest(best, bestCost, ctx);
    sameB.updateBest(best, bestCost, ctx);
    oppoB.updateBest(best, bestCost, ctx);
    for (auto& p : kPawnSq)
        p.updateBest(best, bestCost, ctx);
    pRace.updateBest(best, bestCost, ctx);
    captWdl.updateBest(best, bestCost, ctx);
    for (auto& p : darkSquare)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : fileRankW)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : fileRankB)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : fileDelta)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : rankDelta)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : fileDist)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : rankDist)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : kingDist)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : taxiDist)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : diag)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : forks)
        p.updateBest(best, bestCost, ctx);
    for (auto& p : attacks)
        p.updateBest(best, bestCost, ctx);

    // Adjust counts based on fraction of positions sampled
    struct Visitor : public DT::Visitor {
        Visitor(int nChunks, int appliedChunks) : nChunks(nChunks), appliedChunks(appliedChunks) {}
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node) {
            node.left->accept(*this);
            node.right->accept(*this);
        }
        void visit(DT::StatsNode& node) {
            WDLStatsNode& wdlNode = static_cast<WDLStatsNode&>(node);
            wdlNode.scaleStats(nChunks, appliedChunks);
        }
    private:
        const int nChunks;
        const int appliedChunks;
    };
    Visitor visitor(nChunks, appliedChunks);
    best->accept(visitor);

    return best;
}

// ------------------------------------------------------------

WDLEncoderNode::WDLEncoderNode(const WDLStats& stats, bool approximate) {
    constexpr int N = WDLStats::nWdlVals;
    std::array<std::pair<U64,int>,N> srt;
    const U64 maxVal = ~0ULL;
    for (int i = 0; i < N; i++) {
        int encVal = (approximate || stats.getCount(i)) ? i : -1;
        srt[i] = std::make_pair(maxVal - stats.getCount(i), encVal);
    }
    std::sort(srt.begin(), srt.end());
    for (int i = 0; i < N; i++)
        encTable[i] = srt[i].second;
}

int
WDLEncoderNode::encodeValue(const Position& pos, int value, DT::EvalContext& ctx) const {
    const auto& wdlCtx = static_cast<const WDLEvalContext&>(ctx);
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
WDLEncoderNode::getStats(const DT::EvalContext& ctx) const {
    return make_unique<WDLStatsNode>(WDLStats{});
}

std::string
WDLEncoderNode::describe(int indentLevel, const DT::EvalContext& ctx) const {
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

// ------------------------------------------------------------

std::unique_ptr<DT::StatsCollectorNode>
WDLNodeFactory::makeStatsCollector(DT::EvalContext& ctx, int nChunks) {
    return make_unique<WDLStatsCollectorNode>(ctx, nChunks);
};

std::unique_ptr<DT::EvalContext>
WDLNodeFactory::makeEvalContext(const PosIndex& posIdx) {
    return make_unique<WDLEvalContext>(posIdx, useGiniImpurity, mergeThreshold);
}

// ------------------------------------------------------------

void
WDLEvalContext::init(const Position& pos,
                     const DT::UncompressedData& data, U64 idx) {
    const WDLUncompressedData& wdlData = static_cast<const WDLUncompressedData&>(data);
    captWdl = wdlData.getCaptureWdl(idx);
}
