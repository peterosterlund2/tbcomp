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
WDLStats::costError(bool useGini) const {
    if (useGini) {
        return ::giniImpurityError(count.begin(), count.end());
    } else {
        return ::entropyError(count.begin(), count.end());
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

double
WDLStatsNode::costError(const DT::EvalContext& ctx) const {
    bool useGini = static_cast<const WDLEvalContext&>(ctx).useGini();
    return stats.costError(useGini);
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

WDLStatsCollectorNode::WDLStatsCollectorNode(const DT::EvalContext& ctx, int nChunks)
    : StatsCollectorNode(nChunks) {
    int nPieces = ctx.numPieces();
    for (int i = 0; i < nPieces; i++)
        if (Piece::makeWhite(ctx.getPieceType(i)) == Piece::WPAWN)
            kPawnSq.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        darkSquare.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        fileRankF.emplace_back(i);
    for (int i = 0; i < nPieces; i++)
        fileRankR.emplace_back(i);
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

template <typename Func>
void
WDLStatsCollectorNode::iterateMembers(Func func) {
    func(wtm);
    func(inCheck);
    func(bPairW);
    func(bPairB);
    func(sameB);
    func(oppoB);
    for (auto& p : kPawnSq)
        func(p);
    func(pRace);
    func(captWdl);
    for (auto& p : darkSquare)
        func(p);
    for (auto& p : fileRankF)
        func(p);
    for (auto& p : fileRankR)
        func(p);
    for (auto& p : fileDelta)
        func(p);
    for (auto& p : rankDelta)
        func(p);
    for (auto& p : fileDist)
        func(p);
    for (auto& p : rankDist)
        func(p);
    for (auto& p : kingDist)
        func(p);
    for (auto& p : taxiDist)
        func(p);
    for (auto& p : diag)
        func(p);
    for (auto& p : forks)
        func(p);
    for (auto& p : attacks)
        func(p);
}

template <typename Func>
void
WDLStatsCollectorNode::iterateMembers(Func func) const {
    decltype(func(wtm))* dummy1; (void)dummy1; // Check const
    decltype(func(fileRankF[0]))* dummy2; (void)dummy2;
    (const_cast<WDLStatsCollectorNode&>(*this)).iterateMembers(func);
}

bool
WDLStatsCollectorNode::applyData(const Position& pos, int value, DT::EvalContext& ctx) {
    iterateMembers([&pos,value,&ctx](auto& collector) {
        collector.applyData(pos, ctx, value);
    });
    return true;
}

std::unique_ptr<DT::Node>
WDLStatsCollectorNode::getBest(const DT::EvalContext& ctx) const {
    std::unique_ptr<DT::Node> best;
    double bestCost = DBL_MAX;
    iterateMembers([&](const auto& collector) {
        collector.updateBest(best, bestCost, ctx);
    });

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

double
WDLStatsCollectorNode::costError(const DT::EvalContext& ctx) const {
    std::unique_ptr<DT::Node> best;
    double bestCost = DBL_MAX;
    iterateMembers([&](const auto& collector) {
        collector.updateBest(best, bestCost, ctx);
    });

    struct Visitor : public DT::Visitor {
        Visitor(const DT::EvalContext& ctx) : ctx(ctx) {}
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node) {
            node.left->accept(*this);
            node.right->accept(*this);
        }
        void visit(DT::StatsNode& node) {
            WDLStatsNode& wdlNode = static_cast<WDLStatsNode&>(node);
            double err = wdlNode.costError(ctx);
            err2 += err * err;
        }
        const DT::EvalContext& ctx;
        double err2 = 0;
    };
    Visitor visitor(ctx);
    best->accept(visitor);

    double N = nChunks;
    double n = appliedChunks;
    return sqrt(visitor.err2 * (N - n) / N);
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
WDLNodeFactory::makeStatsCollector(const DT::EvalContext& ctx, int nChunks) {
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
