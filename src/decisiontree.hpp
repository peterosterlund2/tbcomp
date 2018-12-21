#ifndef DECISIONTREE_HPP_
#define DECISIONTREE_HPP_

#include "dtnode.hpp"
#include "util/util.hpp"
#include <vector>

class PosIndex;
class BitArray;
class Position;


/** Class to compute a decision tree that predicts the values in a tablebase. */
class DecisionTree {
public:
    /** "active" contains one bit for each element in data. A bit
     *  is set to false if the corresponding position can be handled
     *  without using a decision tree. */
    DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                 DT::UncompressedData& data, const BitArray& active,
                 int samplingLogFactor);

    /** Compute decision tree having maximum depth "maxDepth",
     *  using "nThreads" threads. */
    void computeTree(int maxDepth, int nThreads);

    /** Create serialized bytecode representation of the tree. */
    void serialize(std::vector<U8>& out);

private:
    /** Update statistics for all MultiPredicate nodes. */
    void updateStats(unsigned int chunkNo);

    /** For all StatsCollectorNodes report that one chunk has been processed. */
    void statsChunkAdded();

    /** For each StatsCollectorNode, if it is accurate enough, replace it with
     *  a tree consisting of the best predicate and two new StatsCollectorNodes.
     *  @return True if there are still StatsCollectorNodes in the tree. */
    bool selectBestPreds(int maxDepth);

    /** Merge nodes if they are equivalent or if the cost change is small enough. */
    void simplifyTree();

    /** Replace StatsNode with EncoderNode. */
    void makeEncoderTree();

    /** Return number of leaf nodes in the decision tree. */
    int getNumLeafNodes();

    /** Apply decision tree encoding for all applicable values in data. */
    void encodeValues(int nThreads);

    /** Log a random sample of mispredicted positions to a file. */
    void logMisPredicted(U64 remaining);

    DT::NodeFactory& nodeFactory;
    const PosIndex& posIdx;
    DT::UncompressedData& data;
    const BitArray& active;

    std::unique_ptr<DT::Node> root;
    int nStatsChunks;     // Must be power of 2
};

#endif
