#include "decisiontree.hpp"
#include "bitarray.hpp"
#include "posindex.hpp"
#include "position.hpp"
#include "threadpool.hpp"
#include <cassert>


DecisionTree::DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                           DT::UncompressedData& data, const BitArray& active)
    : nodeFactory(nodeFactory), posIdx(posIdx), data(data), active(active) {
}

void
DecisionTree::computeTree(int maxDepth, int nThreads) {
    std::unique_ptr<DT::EvalContext> ctx = nodeFactory.makeEvalContext(posIdx);
    root = nodeFactory.makeStatsCollector(*ctx);

    for (int lev = 0; lev < maxDepth; lev++) {
        updateStats();
        std::cout << '\n' << root->describe(0) << "lev:" << lev
                  << " entropy:" << root->entropy()
                  << " numLeafs:" << getNumLeafNodes() << std::endl;

        bool finished = !selectBestPreds(lev + 1 < maxDepth);
        if (finished)
            break;
    }

    simplifyTree();
    std::cout << '\n' << root->describe(0) << "entropy:" << root->entropy()
              << " numLeafs:" << getNumLeafNodes() << std::endl;

    makeEncoderTree();
    std::cout << '\n' << root->describe(0)
              << "numLeafs:" << getNumLeafNodes() << std::endl;

    encodeValues(nThreads);
}

void
DecisionTree::updateStats() {
    U64 nPos = posIdx.tbSize();
    Position pos;
    std::unique_ptr<DT::EvalContext> ctx = nodeFactory.makeEvalContext(posIdx);
    for (U64 idx = 0; idx < nPos; idx++) {
        if (!active.get(idx) || data.isHandled(idx))
            continue;

        for (U64 m = pos.occupiedBB(); m; ) // Clear position
            pos.clearPiece(BitBoard::extractSquare(m));
        bool valid = posIdx.index2Pos(idx, pos);
        assert(valid);
        ctx->init(pos, data, idx);

        int value = data.getValue(idx);
        if (!root->applyData(pos, value, *ctx))
            data.setHandled(idx, true);
    }
}

bool
DecisionTree::selectBestPreds(bool createNewStatsCollector) {
    class Visitor : public DT::Visitor {
    public:
        Visitor(bool createNewStatsCollector, DT::NodeFactory& nodeFactory,
                DT::EvalContext& ctx) :
            createNewStatsCollector(createNewStatsCollector), nodeFactory(nodeFactory), ctx(ctx) {}
        void visit(DT::StatsCollectorNode& node, std::unique_ptr<DT::Node>& owner) override {
            owner = node.getBest();
            if (createNewStatsCollector) {
                DT::PredicateNode* predNode = dynamic_cast<DT::PredicateNode*>(owner.get());
                if (predNode) {
                    if (predNode->left->entropy() > 0) {
                        predNode->left = nodeFactory.makeStatsCollector(ctx);
                        anyStatsCollectorCreated = true;
                    }
                    if (predNode->right->entropy() > 0) {
                        predNode->right = nodeFactory.makeStatsCollector(ctx);
                        anyStatsCollectorCreated = true;
                    }
                }
            }
        }
        const bool createNewStatsCollector;
        DT::NodeFactory& nodeFactory;
        DT::EvalContext& ctx;
        bool anyStatsCollectorCreated = false;
    };
    std::unique_ptr<DT::EvalContext> ctx = nodeFactory.makeEvalContext(posIdx);
    Visitor visitor(createNewStatsCollector, nodeFactory, *ctx);
    root->accept(visitor, root);
    return visitor.anyStatsCollectorCreated;
}

void
DecisionTree::simplifyTree() {
    class Visitor : public DT::Visitor {
        void visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner) override {
            node.left->accept(*this, node.left);
            node.right->accept(*this, node.right);

            DT::StatsNode* left = dynamic_cast<DT::StatsNode*>(node.left.get());
            DT::StatsNode* right = dynamic_cast<DT::StatsNode*>(node.right.get());
            if (left && right) {
                std::unique_ptr<DT::StatsNode> merged = left->mergeWithNode(*right);
                if (merged)
                    owner = std::move(merged);
            }
        }
    };
    Visitor visitor;
    root->accept(visitor, root);
}

void
DecisionTree::makeEncoderTree() {
    class Visitor : public DT::Visitor {
    public:
        void visit(DT::StatsNode& node, std::unique_ptr<DT::Node>& owner) override {
            owner = node.getEncoder();
        }
    };
    Visitor visitor;
    root->accept(visitor, root);
}

int
DecisionTree::getNumLeafNodes() {
    class Visitor : public DT::Visitor {
    public:
        void visit(DT::StatsNode& node, std::unique_ptr<DT::Node>& owner) override {
            nLeafs++;
        }
        void visit(DT::StatsCollectorNode& node, std::unique_ptr<DT::Node>& owner) override {
            nLeafs++;
        }
        void visit(DT::EncoderNode& node, std::unique_ptr<DT::Node>& owner) override {
            nLeafs++;
        }
        int nLeafs = 0;
    };
    Visitor visitor;
    root->accept(visitor, root);
    return visitor.nLeafs;
}

void
DecisionTree::encodeValues(int nThreads) {
    const U64 size = posIdx.tbSize();
    const U64 batchSize = std::max((U64)128*1024, (size + 1023) / 1024);
    ThreadPool<int> pool(nThreads);
    for (U64 b = 0; b < size; b += batchSize) {
        auto task = [this,size,batchSize,b](int workerNo) {
            Position pos;
            std::unique_ptr<DT::EvalContext> ctx = nodeFactory.makeEvalContext(posIdx);
            U64 end = std::min(b + batchSize, size);
            for (U64 idx = b; idx < end; idx++) {
                if (!active.get(idx))
                    continue;

                for (U64 m = pos.occupiedBB(); m; ) // Clear position
                    pos.clearPiece(BitBoard::extractSquare(m));
                bool valid = posIdx.index2Pos(idx, pos);
                assert(valid);
                ctx->init(pos, data, idx);

                int value = data.getValue(idx);
                int encVal = root->encodeValue(pos, value, *ctx);
                data.setEncoded(idx, encVal);
            }
            return 0;
        };
        pool.addTask(task);
    }
    int dummy;
    while (pool.getResult(dummy))
        ;
}

void
DecisionTree::serialize(std::vector<U8>& out) {

}
