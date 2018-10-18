#ifndef DECISIONTREE_HPP_
#define DECISIONTREE_HPP_

#include "tbutil.hpp"
#include "predicate.hpp"
#include <memory>

class PosIndex;
class BitArray;


class DecisionTree {
public:
    /** "active" contains one bit for each element in data. The bit
     *  is set to false when the corresponding position has been handled,
     *  i.e. it corresponds to an invalid position or a leaf node in the
     *  decision tree. */
    DecisionTree(PredicateFactory& predFactory, const PosIndex& posIdx,
                 std::vector<U8>& data, BitArray& active);

    /** Compute decision tree having maximum depth "maxDepth",
     *  using "nThreads" threads. */
    void computeTree(int maxDepth, int nThreads);

    /** Create serialized bytecode representation of the tree. */
    void serialize(std::vector<U8>& out);

private:
    /** Update statistics for all MultiPredicate nodes. */
    void updateStats();
    /** For each MultiPredicate nodes, replace them with the best corresponding
     *  SinglePredicate and optionally create new MultiPredicate nodes for the
     *  left/right children.
     *  @return True if an MultiPredicate was created. */
    bool selectBestPreds(bool createNewMultiPred);

    struct DTNode;
    void printTree(const DTNode* node, int indentLevel);

    PredicateFactory& predFactory;
    const PosIndex& posIdx;
    std::vector<U8>& data;
    BitArray& active;

    struct DTNode {
        std::unique_ptr<Predicate> pred;
        std::unique_ptr<DTNode> left;   // Left child (pred true) or nullptr
        std::unique_ptr<DTNode> right;  // Right child (pred false) or nullptr
    };
    DTNode root;
};

#endif
