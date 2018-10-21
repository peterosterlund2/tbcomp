#include "decisiontree.hpp"
#include "bitarray.hpp"
#include "posindex.hpp"
#include "position.hpp"
#include <cassert>


DecisionTree::DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                           std::vector<U8>& data, const BitArray& active)
    : nodeFactory(nodeFactory), posIdx(posIdx), data(data), active(active) {
}

void
DecisionTree::computeTree(int maxDepth, int nThreads) {
    root = nodeFactory.makeStatsCollector();

    for (int lev = 0; lev < maxDepth; lev++) {
        updateStats();
        std::cout << '\n' << root->describe(0) << std::flush;

        bool finished = !selectBestPreds(lev + 1 < maxDepth);
        if (finished)
            break;
    }
    std::cout << "entropy:" << root->entropy() << std::endl;

    makeEncoderTree();
    std::cout << '\n' << root->describe(0) << std::flush;

}

void
DecisionTree::updateStats() {
    U64 nPos = posIdx.tbSize();
    Position pos;
    for (U64 idx = 0; idx < nPos; idx++) {
        if (!active.get(idx))
            continue;

        for (U64 m = pos.occupiedBB(); m; ) // Clear position
            pos.clearPiece(BitBoard::extractSquare(m));
        bool valid = posIdx.index2Pos(idx, pos);
        assert(valid);

        int value = (S8)data[idx];
        root->applyData(pos, value);
    }
}

bool
DecisionTree::selectBestPreds(bool createNewStatsCollector) {
    class Visitor : public DT::Visitor {
    public:
        Visitor(bool createNewStatsCollector, DT::NodeFactory& nodeFactory) :
            createNewStatsCollector(createNewStatsCollector), nodeFactory(nodeFactory) {}
        void visit(DT::StatsCollectorNode& node, std::unique_ptr<DT::Node>& owner) override {
            if (createNewStatsCollector) {
                owner = node.getBest();
                DT::PredicateNode* predNode = dynamic_cast<DT::PredicateNode*>(owner.get());
                if (predNode) {
                    if (predNode->left->entropy() > 0) {
                        predNode->left = nodeFactory.makeStatsCollector();
                        anyStatsCollectorCreated = true;
                    }
                    if (predNode->right->entropy() > 0) {
                        predNode->right = nodeFactory.makeStatsCollector();
                        anyStatsCollectorCreated = true;
                    }
                }
            }
        }
        const bool createNewStatsCollector;
        DT::NodeFactory& nodeFactory;
        bool anyStatsCollectorCreated = false;
    };
    Visitor visitor(createNewStatsCollector, nodeFactory);
    root->accept(visitor, root);
    return visitor.anyStatsCollectorCreated;
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

void
DecisionTree::serialize(std::vector<U8>& out) {

}
