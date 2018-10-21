#include "decisiontree.hpp"
#include "bitarray.hpp"
#include "posindex.hpp"
#include "position.hpp"
#include <functional>
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
    bool anyStatsCollectorCreated = false;
    std::vector<std::reference_wrapper<std::unique_ptr<DT::Node>>> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        std::unique_ptr<DT::Node>& node = stack.back().get();
        stack.pop_back();
        DT::PredicateNode* predNode = dynamic_cast<DT::PredicateNode*>(node.get());
        if (predNode) {
            stack.push_back(predNode->right);
            stack.push_back(predNode->left);
        } else if (createNewStatsCollector) {
            auto sCollNode = dynamic_cast<DT::StatsCollectorNode*>(node.get());
            if (sCollNode) {
                node = sCollNode->getBest();
                predNode = dynamic_cast<DT::PredicateNode*>(node.get());
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
    }
    return anyStatsCollectorCreated;
}

void
DecisionTree::serialize(std::vector<U8>& out) {

}
