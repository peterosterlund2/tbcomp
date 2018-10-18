#include "decisiontree.hpp"
#include "bitarray.hpp"
#include "posindex.hpp"
#include "position.hpp"
#include <cassert>


DecisionTree::DecisionTree(PredicateFactory& predFactory, const PosIndex& posIdx,
                           std::vector<U8>& data, BitArray& active)
    : predFactory(predFactory), posIdx(posIdx), data(data), active(active) {
}

void
DecisionTree::computeTree(int maxDepth, int nThreads) {
    root.pred = predFactory.makeMultiPredicate();

    for (int lev = 0; lev < maxDepth; lev++) {
        updateStats();
        bool finished = !selectBestPreds(lev + 1 < maxDepth);

        std::cout << std::endl;
        printTree(&root, 0);
        if (finished)
            break;
    }
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
        if (!valid)
            continue;
        int value = (S8)data[idx];

        DTNode* node = &root;
        while (node) {
            Predicate::ApplyResult res = node->pred->applyData(pos, value);
            switch (res) {
            case Predicate::PRED_TRUE:
                node = node->left.get();
                if (!node)
                    active.set(idx, false);
                break;
            case Predicate::PRED_FALSE:
                node = node->right.get();
                if (!node)
                    active.set(idx, false);
                break;
            case Predicate::PRED_NONE:
                node = nullptr;
                break;
            }
        }
    }
}

bool
DecisionTree::selectBestPreds(bool createNewMultiPred) {
    bool anyMultiCreated = false;
    struct Data {
        DTNode* node;
        DTNode* parent;
        bool rightChild;
    };
    std::vector<Data> stack;
    stack.push_back(Data{&root, nullptr, false});
    while (!stack.empty()) {
        Data data = stack.back();
        DTNode* node = data.node;
        stack.pop_back();
        SinglePredicate* sPred = dynamic_cast<SinglePredicate*>(node->pred.get());
        if (sPred) {
            if (node->right)
                stack.push_back(Data{node->right.get(), node, true});
            if (node->left)
                stack.push_back(Data{node->left.get(), node, false});
        } else {
            MultiPredicate* mPred = dynamic_cast<MultiPredicate*>(node->pred.get());
            assert(mPred);
            node->pred = mPred->getBest();
            sPred = dynamic_cast<SinglePredicate*>(node->pred.get());
            assert(sPred);
            if (sPred->isUseful()) {
                if (createNewMultiPred) {
                    if (sPred->entropy(true) > 0) {
                        node->left = make_unique<DTNode>();
                        node->left->pred = predFactory.makeMultiPredicate();
                        anyMultiCreated = true;
                    }
                    if (sPred->entropy(false) > 0) {
                        node->right = make_unique<DTNode>();
                        node->right->pred = predFactory.makeMultiPredicate();
                        anyMultiCreated = true;
                    }
                }
            } else if (data.parent) {
                if (data.rightChild)
                    data.parent->right = nullptr;
                else
                    data.parent->left = nullptr;
            }
        }
    }
    return anyMultiCreated;
}

void
DecisionTree::printTree(const DTNode* node, int indentLevel) {
    std::string indent(indentLevel*2, ' ');
    const SinglePredicate* pred = dynamic_cast<const SinglePredicate*>(node->pred.get());
    if (pred) {
        std::cout << indent << pred->describe() << std::endl;
        if (node->left.get()) {
            printTree(node->left.get(), indentLevel + 1);
        } else {
            std::cout << indent << "  " << pred->describeChild(true) << std::endl;
        }
        if (node->right.get()) {
            printTree(node->right.get(), indentLevel + 1);
        } else {
            std::cout << indent << "  " << pred->describeChild(false) << std::endl;
        }
    } else {
        std::cout << indent << "--" << std::endl;
    }
}

void
DecisionTree::serialize(std::vector<U8>& out) {

}
