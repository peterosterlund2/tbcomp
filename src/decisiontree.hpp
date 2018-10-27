#ifndef DECISIONTREE_HPP_
#define DECISIONTREE_HPP_

#include "dtnode.hpp"
#include "util/util.hpp"
#include <vector>

class PosIndex;
class BitArray;
class Position;


/** Abstract representation of uncompressed data for a tablebase. */
class UncompressedData {
public:
    virtual ~UncompressedData() = default;

    virtual int getValue(U64 idx) const = 0;
    virtual void setValue(U64 idx, int value) = 0;

    virtual bool isActive(U64 idx) const = 0;
    virtual void setActive(U64 idx, bool active) = 0;
};

/** Class to compute a decision tree that predicts the values in a tablebase. */
class DecisionTree {
public:
    /** "active" contains one bit for each element in data. A bit
     *  is set to false if the corresponding position can be handled
     *  without using a decision tree. */
    DecisionTree(DT::NodeFactory& nodeFactory, const PosIndex& posIdx,
                 UncompressedData& data, const BitArray& active);

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

    /** Merge nodes if they are equivalent or if the entropy change is small enough. */
    void simplifyTree();

    /** Replace StatsNode with EncoderNode. */
    void makeEncoderTree();

    /** Apply decision tree encoding for all applicable values in data. */
    void encodeValues();

    DT::NodeFactory& nodeFactory;
    const PosIndex& posIdx;
    UncompressedData& data;
    const BitArray& active;

    std::unique_ptr<DT::Node> root;
};

#endif
