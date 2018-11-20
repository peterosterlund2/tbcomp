#ifndef DTNODE_HPP_
#define DTNODE_HPP_

#include "util/util.hpp"
#include "posindex.hpp"
#include "predicate.hpp"


namespace DT {

class PredicateNode;
class StatsNode;
class StatsCollectorNode;
class EncoderNode;
class EvalContext;


class Node {
protected:
    enum class NodeType : int {
        PREDICATE,
        STATSCOLLECTOR,
        STATS,
        ENCODER
    };
public:
    explicit Node(NodeType type) : nodeType(type) {}
    virtual ~Node() = default;

    /** Sum of entropy for all nodes in the tree. */
    virtual double entropy() const = 0;

    /** Get statistics for this node. */
    virtual std::unique_ptr<StatsNode> getStats() const = 0;

    /** Template based visitor pattern. */
    template <typename Visitor, typename... Args>
    void accept(Visitor& visitor, Args&&... args);

    /** Text description of tree rooted at this node. For debugging. */
    virtual std::string describe(int indentLevel) const = 0;

private:
    const NodeType nodeType;
};

class Visitor {
public:
    template <typename... Args> void visit(DT::PredicateNode& node, Args&&... args) {}
    template <typename... Args> void visit(DT::StatsNode& node, Args&&... args) {}
    template <typename... Args> void visit(DT::StatsCollectorNode& node, Args&&...) {}
    template <typename... Args> void visit(DT::EncoderNode& node, Args&&... args) {}
};

/** Abstract representation of uncompressed data for a tablebase. */
class UncompressedData {
public:
    virtual ~UncompressedData() = default;

    virtual int getValue(U64 idx) const = 0;
    virtual void setEncoded(U64 idx, int value) = 0;
    virtual int getEncoded(U64 idx) const = 0;

    virtual bool isHandled(U64 idx) const = 0;
    virtual void setHandled(U64 idx, bool active) = 0;
};

class EvalContext {
public:
    EvalContext(const PosIndex& posIdx) : posIdx(posIdx) {}
    ~EvalContext() = default;

    virtual void init(const Position& pos, const UncompressedData& data, U64 idx) = 0;

    int numPieces() const;
    Piece::Type getPieceType(int pieceNo) const;
    int getPieceSquare(int pieceNo, const Position& pos) const;

protected:
    const PosIndex& posIdx;
};

class PredicateNode : public Node {
public:
    PredicateNode();

    /** Get left/right child depending on the predicate. */
    Node& getChild(const Position& pos, EvalContext& ctx);
    double entropy() const override;
    std::unique_ptr<StatsNode> getStats() const override;
    std::string describe(int indentLevel) const override;

    std::unique_ptr<Predicate> pred;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

class StatsNode : public Node {
public:
    StatsNode() : Node(NodeType::STATS) {}

    /** Add statistics from "other" to this node. */
    virtual void addStats(const StatsNode* other) = 0;

    /** Return true if no positions correspond to this node. */
    virtual bool isEmpty() const = 0;

    /** If merging this with "other" improves the tree, return
     *  the merged node. Otherwise return nullptr. */
    virtual std::unique_ptr<StatsNode> mergeWithNode(const StatsNode& other) const = 0;

    /** Get encoder node corresponding to the statistics in this node. */
    virtual std::unique_ptr<EncoderNode> getEncoder() const = 0;
};

/** A collection of all possible predicates. Collects statistics about
 *  how successful each predicate is at reducing the entropy of the data. */
class StatsCollectorNode : public Node {
public:
    StatsCollectorNode() : Node(NodeType::STATSCOLLECTOR) {}

    /** Apply position value to tree, possibly by delegating to a child node.
     *  @return True if application was successful, false otherwise. */
    virtual bool applyData(const Position& pos, int value, EvalContext& ctx) = 0;

    double entropy() const override;
    std::unique_ptr<StatsNode> getStats() const override;
    std::string describe(int indentLevel) const override;

    /** Create node that realizes the largest information gain. */
    virtual std::unique_ptr<Node> getBest() const = 0;
};

class EncoderNode : public Node {
public:
    EncoderNode() : Node(NodeType::ENCODER) {}

    /** Encode a value based on decision tree prediction. Most likely value
     *  encoded as 0, second most likely as 1, etc. */
    virtual int encodeValue(const Position& pos, int value, EvalContext& ctx) const = 0;

    double entropy() const override;
};

class NodeFactory {
public:
    virtual std::unique_ptr<StatsCollectorNode> makeStatsCollector(EvalContext& ctx) = 0;

    virtual std::unique_ptr<EvalContext> makeEvalContext(const PosIndex& posIdx) = 0;
};

template <typename Visitor, typename... Args>
void Node::accept(Visitor& visitor, Args&&... args) {
    switch (nodeType) {
    case NodeType::PREDICATE:
        visitor.visit(static_cast<DT::PredicateNode&>(*this), std::forward<Args>(args)...);
        break;
    case NodeType::STATSCOLLECTOR:
        visitor.visit(static_cast<DT::StatsCollectorNode&>(*this), std::forward<Args>(args)...);
        break;
    case NodeType::STATS:
        visitor.visit(static_cast<DT::StatsNode&>(*this), std::forward<Args>(args)...);
        break;
    case NodeType::ENCODER:
        visitor.visit(static_cast<DT::EncoderNode&>(*this), std::forward<Args>(args)...);
        break;
    default:
        assert(false);
        break;
    }
}

inline int
EvalContext::numPieces() const {
    return posIdx.numPieces();
}

inline Piece::Type
EvalContext::getPieceType(int pieceNo) const {
    return posIdx.getPieceType(pieceNo);
}

inline int
EvalContext::getPieceSquare(int pieceNo, const Position& pos) const {
    return posIdx.getPieceSquare(pieceNo, pos);
}

inline Node&
PredicateNode::getChild(const Position& pos, EvalContext& ctx) {
    return *(pred->eval(pos, ctx) ? right : left);
}

}

#endif /* DTNODE_HPP_ */
