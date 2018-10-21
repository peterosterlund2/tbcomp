#ifndef DTNODE_HPP_
#define DTNODE_HPP_

#include "tbutil.hpp"
#include "predicate.hpp"
#include <memory>


namespace DT {

class PredicateNode;
class StatsNode;
class StatsCollectorNode;
class EncoderNode;
class Visitor;

class Node {
public:
    virtual ~Node() = default;

    /** Apply position value to tree, possibly by delegating to a child node.
     *  @return True if application was successful, false otherwise. */
    virtual bool applyData(const Position& pos, int value) = 0;

    /** Encode a value based on decision tree prediction. Most likely value
     *  encoded as 0, second most likely as 1, etc. */
    virtual int encodeValue(const Position& pos, int value) const = 0;

    /** Sum of entropy for all nodes in the tree. */
    virtual double entropy() const = 0;

    /** Get statistics for this node. */
    virtual std::unique_ptr<StatsNode> getStats() const = 0;

    /** Visitor pattern. */
    virtual void accept(Visitor& visitor, std::unique_ptr<Node>& owner) = 0;

    /** Text description of tree rooted at this node. For debugging. */
    virtual std::string describe(int indentLevel) const = 0;
};

/** The visitor is allowed to modify the tree by overwriting "owner", but
 *  this invalidates the "node" reference, so special care is needed. */
class Visitor {
public:
    /** Default implementation visits left and right child nodes. */
    virtual void visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner);
    virtual void visit(DT::StatsNode& node, std::unique_ptr<DT::Node>& owner) { }
    virtual void visit(DT::StatsCollectorNode& node, std::unique_ptr<DT::Node>& owner) { }
    virtual void visit(DT::EncoderNode& node, std::unique_ptr<DT::Node>& owner) { }
};

class PredicateNode : public Node {
public:
    bool applyData(const Position& pos, int value) override;
    int encodeValue(const Position& pos, int value) const override;
    double entropy() const override;
    std::unique_ptr<StatsNode> getStats() const override;
    void accept(Visitor& visitor, std::unique_ptr<Node>& owner) override;
    std::string describe(int indentLevel) const override;

    std::unique_ptr<Predicate> pred;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

class StatsNode : public Node {
public:
    bool applyData(const Position& pos, int value) override;
    int encodeValue(const Position& pos, int value) const override;
    void accept(Visitor& visitor, std::unique_ptr<Node>& owner) override;

    /** Add statistics from "other" to this node. */
    virtual void addStats(const StatsNode* other) = 0;

    /** Return true if no positions correspond to this node. */
    virtual bool isEmpty() const = 0;

    /** Get encoder node corresponding to the statistics in this node. */
    virtual std::unique_ptr<EncoderNode> getEncoder() = 0;
};

/** A collection of all possible predicates. Collects statistics about
 *  how successful each predicate is at reducing the entropy of the data. */
class StatsCollectorNode : public Node {
public:
    int encodeValue(const Position& pos, int value) const override;
    double entropy() const override;
    std::unique_ptr<StatsNode> getStats() const override;
    void accept(Visitor& visitor, std::unique_ptr<Node>& owner) override;
    std::string describe(int indentLevel) const override;

    /** Create node that realizes the largest information gain. */
    virtual std::unique_ptr<Node> getBest() const = 0;
};

class EncoderNode : public Node {
public:
    double entropy() const override;
    bool applyData(const Position& pos, int value) override;
    void accept(Visitor& visitor, std::unique_ptr<Node>& owner) override;
};

class NodeFactory {
public:
    virtual std::unique_ptr<StatsCollectorNode> makeStatsCollector() = 0;
};

}

#endif /* DTNODE_HPP_ */
