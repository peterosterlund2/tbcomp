#ifndef DECISIONTREE_HPP_
#define DECISIONTREE_HPP_

#include "tbutil.hpp"
#include "dtnode.hpp"

class PosIndex;
class BitArray;
class Position;


class DecisionTree {
public:
    /** "active" contains one bit for each element in data. A bit
     *  is set to false if the corresponding position can be handled
     *  without using a decision tree. */
    DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                 std::vector<U8>& data, const BitArray& active);

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
    bool selectBestPreds(bool createNewStatsCollector);

    /** Replace StatsNode with EncoderNode. */
    void makeEncoderTree();

    DT::NodeFactory& nodeFactory;
    const PosIndex& posIdx;
    std::vector<U8>& data;
    const BitArray& active;

    std::unique_ptr<DT::Node> root;
};

#endif
