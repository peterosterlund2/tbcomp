#include "decisiontree.hpp"
#include "predicate.hpp"
#include "bitarray.hpp"
#include "posindex.hpp"
#include "position.hpp"
#include "textio.hpp"
#include "util/random.hpp"
#include "util/timeUtil.hpp"
#include "threadpool.hpp"
#include "tbutil.hpp"
#include <fstream>
#include <cassert>


DecisionTree::DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                           DT::UncompressedData& data, const BitArray& active,
                           int samplingLogFactor)
    : nodeFactory(nodeFactory), posIdx(posIdx), data(data), active(active) {
    nStatsChunks = 1 << samplingLogFactor;
}

void
DecisionTree::computeTree(int maxDepth, int nThreads) {
    auto ctx = nodeFactory.makeEvalContext(posIdx);
    root = nodeFactory.makeStatsCollector(*ctx, nStatsChunks);

    S64 t0 = currentTimeMillis();
    for (int lev = 0; lev < maxDepth; lev++) {
        updateStats();
        std::cout << "lev:" << lev << " cost:" << root->cost(*ctx)
                  << " numLeafs:" << getNumLeafNodes() << std::endl;

        bool finished = !selectBestPreds(lev + 1 < maxDepth);
        if (finished)
            break;
    }
    S64 t1 = currentTimeMillis();

    simplifyTree();
    double cost = root->cost(*ctx);
    std::cout << '\n' << root->describe(0, *ctx) << std::endl;

    makeEncoderTree();
    std::cout << '\n' << root->describe(0, *ctx) << "cost:" << cost
              << " numLeafs:" << getNumLeafNodes() << std::endl;
    std::cout << "time:" << (t1 -t0) * 1e-3 << std::endl;

    encodeValues(nThreads);
}

void
DecisionTree::updateStats() {
    U64 nPos = posIdx.tbSize();
    Position pos;
    auto ctx = nodeFactory.makeEvalContext(posIdx);

    struct Visitor {
        Visitor(const Position& pos, DT::EvalContext& ctx) : pos(pos), ctx(ctx) {}
        void visit(DT::PredicateNode& node) {
            return node.getChild(pos, ctx).accept(*this);
        }
        void visit(DT::StatsNode& node) {
            result = false;
        }
        void visit(DT::StatsCollectorNode& node) {
            result = node.applyData(pos, value, ctx);
        }
        void visit(DT::EncoderNode& node) {
            assert(false);
        }
        const Position& pos;
        DT::EvalContext& ctx;
        int value = 0;
        bool result = false;
    };
    Visitor visitor(pos, *ctx);

    for (U64 idx = 0; idx < nPos; idx++) {
        if ((hashU64(idx) & (nStatsChunks - 1)) != 0)
            continue;
        if (!active.get(idx) || data.isHandled(idx))
            continue;

        bool valid = posIdx.index2Pos(idx, pos);
        assert(valid);
        ctx->init(pos, data, idx);

        visitor.value = data.getValue(idx);
        root->accept(visitor);
        if (!visitor.result)
            data.setHandled(idx, true);
    }

    statsChunkAdded();
}

void
DecisionTree::statsChunkAdded() {
    struct Visitor : public DT::Visitor {
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node) {
            node.left->accept(*this);
            node.right->accept(*this);
        }
        void visit(DT::StatsCollectorNode& node) {
            node.chunkAdded();
        }
    };
    Visitor visitor;
    root->accept(visitor);
}

bool
DecisionTree::selectBestPreds(bool createNewStatsCollector) {
    struct Visitor : public DT::Visitor {
        Visitor(bool createNewStatsCollector, DT::NodeFactory& nodeFactory,
                DT::EvalContext& ctx, int nStatsChunks) :
            createNewStatsCollector(createNewStatsCollector), nodeFactory(nodeFactory), ctx(ctx),
            nStatsChunks(nStatsChunks) {}
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner) {
            node.left->accept(*this, node.left);
            node.right->accept(*this, node.right);
        }
        void visit(DT::StatsCollectorNode& node, std::unique_ptr<DT::Node>& owner) {
            owner = node.getBest(ctx);
            if (createNewStatsCollector) {
                DT::PredicateNode* predNode = dynamic_cast<DT::PredicateNode*>(owner.get());
                if (predNode) {
                    if (predNode->left->cost(ctx) > ctx.getMergeThreshold()) {
                        predNode->left = nodeFactory.makeStatsCollector(ctx, nStatsChunks);
                        anyStatsCollectorCreated = true;
                    }
                    if (predNode->right->cost(ctx) > ctx.getMergeThreshold()) {
                        predNode->right = nodeFactory.makeStatsCollector(ctx, nStatsChunks);
                        anyStatsCollectorCreated = true;
                    }
                }
            }
        }
        const bool createNewStatsCollector;
        DT::NodeFactory& nodeFactory;
        const DT::EvalContext& ctx;
        const int nStatsChunks;
        bool anyStatsCollectorCreated = false;
    };
    auto ctx = nodeFactory.makeEvalContext(posIdx);
    Visitor visitor(createNewStatsCollector, nodeFactory, *ctx, nStatsChunks);
    root->accept(visitor, root);
    return visitor.anyStatsCollectorCreated;
}

void
DecisionTree::simplifyTree() {
    struct Visitor : public DT::Visitor {
        explicit Visitor(const DT::EvalContext& ctx) : ctx(ctx) {}
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner) {
            node.left->accept(*this, node.left);
            node.right->accept(*this, node.right);

            DT::StatsNode* left = dynamic_cast<DT::StatsNode*>(node.left.get());
            DT::StatsNode* right = dynamic_cast<DT::StatsNode*>(node.right.get());
            if (left && right) {
                std::unique_ptr<DT::StatsNode> merged = left->mergeWithNode(*right, ctx);
                if (merged)
                    owner = std::move(merged);
            }
        }
    private:
        const DT::EvalContext& ctx;
    };
    auto ctx = nodeFactory.makeEvalContext(posIdx);
    Visitor visitor(*ctx);
    root->accept(visitor, root);
}

void
DecisionTree::makeEncoderTree() {
    struct Visitor : public DT::Visitor {
        using DT::Visitor::visit;
        void visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner) {
            node.left->accept(*this, node.left);
            node.right->accept(*this, node.right);
        }
        void visit(DT::StatsNode& node, std::unique_ptr<DT::Node>& owner) {
            owner = node.getEncoder();
        }
    };
    Visitor visitor;
    root->accept(visitor, root);
}

int
DecisionTree::getNumLeafNodes() {
    struct Visitor {
        explicit Visitor(const DT::EvalContext& ctx) : ctx(ctx) {}
        void visit(DT::PredicateNode& node) {
            node.left->accept(*this);
            node.right->accept(*this);
        }
        void visit(DT::StatsNode& node) {
            nLeafs++;
        }
        void visit(DT::StatsCollectorNode& node) {
            node.getBest(ctx)->accept(*this);
        }
        void visit(DT::EncoderNode& node) {
            nLeafs++;
        }
        int nLeafs = 0;
    private:
        const DT::EvalContext& ctx;
    };
    auto ctx = nodeFactory.makeEvalContext(posIdx);
    Visitor visitor(*ctx);
    root->accept(visitor);
    return visitor.nLeafs;
}

void
DecisionTree::encodeValues(int nThreads) {
    const U64 size = posIdx.tbSize();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<std::vector<U64>> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [this,size,batchSize,b](int workerNo) {
            Position pos;
            auto ctx = nodeFactory.makeEvalContext(posIdx);
            struct Visitor {
                Visitor(const Position& pos, DT::EvalContext& ctx) : pos(pos), ctx(ctx) {}
                void visit(DT::PredicateNode& node) {
                    return node.getChild(pos, ctx).accept(*this);
                }
                void visit(DT::StatsNode& node) {
                    assert(false);
                }
                void visit(DT::StatsCollectorNode& node) {
                    assert(false);
                }
                void visit(DT::EncoderNode& node) {
                    value = node.encodeValue(pos, value, ctx);
                }
                const Position& pos;
                DT::EvalContext& ctx;
                int value = 0;
            };
            Visitor visitor(pos, *ctx);

            U64 end = std::min(b + batchSize, size);
            std::vector<U64> hist;
            for (U64 idx = b; idx < end; idx++) {
                if (!active.get(idx))
                    continue;

                bool valid = posIdx.index2Pos(idx, pos);
                assert(valid);
                ctx->init(pos, data, idx);

                visitor.value = data.getValue(idx);
                root->accept(visitor);
                int encVal = visitor.value;
                if (encVal >= 0) {
                    if ((int)hist.size() < encVal + 1)
                        hist.resize(encVal + 1);
                    hist[encVal]++;
                }
                data.setEncoded(idx, encVal);
            }
            return hist;
        };
        pool.addTask(task);
    }
    std::vector<U64> hist, tmp;
    while (pool.getResult(tmp)) {
        if (tmp.size() > hist.size())
            hist.resize(tmp.size());
        for (int i = 0; i < (int)tmp.size(); i++)
            hist[i] += tmp[i];
    }
    std::cout << "Encoder histogram:" << std::endl;
    U64 nMisPredicted = 0;
    for (int i = 0; i < (int)hist.size(); i++) {
        if (i > 0)
            nMisPredicted += hist[i];
        std::cout << i << " " << hist[i] << std::endl;
    }

    logMisPredicted(nMisPredicted);
}

void
DecisionTree::logMisPredicted(U64 remaining) {
    Random rndGen(currentTimeMillis());
    U64 nToLog = 1000;
    std::vector<U64> toLog;
    for (U64 idx = 0, size = posIdx.tbSize(); idx < size; idx++) {
        if (data.getEncoded(idx) != 0) {
            if (nToLog >= remaining || rndGen.nextU64() <= (~0ULL) / remaining * nToLog) {
                toLog.push_back(idx);
                nToLog--;
            }
            remaining--;
        }
    }

    int N = toLog.size();
    for (int i = 0; i < N; i++) {
        int idx = i + rndGen.nextInt(N - i);
        std::swap(toLog[i], toLog[idx]);
    }

    std::ofstream of("mispredict.txt");
    Position pos;
    for (U64 idx : toLog) {
        bool valid = posIdx.index2Pos(idx, pos);
        assert(valid);
        of << "idx:" << idx << " val:" << data.getEncoded(idx)
           << " fen:" << TextIO::toFEN(pos) << '\n' << TextIO::asciiBoard(pos);
    }
}

void
DecisionTree::serialize(std::vector<U8>& out) {

}
