#include "dtnode.hpp"
#include <cassert>


namespace DT {

void
Visitor::visit(DT::PredicateNode& node, std::unique_ptr<DT::Node>& owner) {
    node.left->accept(*this, node.left);
    node.right->accept(*this, node.right);
}

// ------------------------------------------------------------

bool
PredicateNode::applyData(const Position& pos, int value) {
    return (pred->eval(pos) ? right : left)->applyData(pos, value);
}

int
PredicateNode::encodeValue(const Position& pos, int value) const {
    return (pred->eval(pos) ? right : left)->encodeValue(pos, value);
}

double
PredicateNode::entropy() const {
    return left->entropy() + right->entropy();
}

std::unique_ptr<StatsNode>
PredicateNode::getStats() const {
    std::unique_ptr<StatsNode> s1 = left->getStats();
    std::unique_ptr<StatsNode> s2 = right->getStats();
    s1->addStats(s2.get());
    return std::move(s1);
}

void
PredicateNode::accept(Visitor& visitor, std::unique_ptr<Node>& owner) {
    visitor.visit(*this, owner);
}

std::string
PredicateNode::describe(int indentLevel) const {
    std::unique_ptr<StatsNode> s1 = left->getStats();
    std::unique_ptr<StatsNode> s2 = right->getStats();
    double entropyAfterPred = s1->entropy() + s2->entropy();

    s1->addStats(s2.get());

    std::stringstream ss;
    if (s1->isEmpty()) {
        ss << std::string(indentLevel*2, ' ');
        ss << pred->name() << '\n';
    } else {
        std::string statsStr = s1->describe(indentLevel);
        ss << statsStr.substr(0, statsStr.length() - 1) << ' ';

        std::stringstream ss2;
        ss2 << std::scientific << std::setprecision(2) << entropyAfterPred;
        ss << pred->name() << ' ' << ss2.str() << '\n';
    }

    ss << left->describe(indentLevel + 1);
    ss << right->describe(indentLevel + 1);
    return ss.str();
}

// ------------------------------------------------------------

bool
StatsNode::applyData(const Position& pos, int value) {
    return false;
}

int
StatsNode::encodeValue(const Position& pos, int value) const {
    assert(false);
    return 0;
}

void
StatsNode::accept(Visitor& visitor, std::unique_ptr<Node>& owner) {
    visitor.visit(*this, owner);
}

// ------------------------------------------------------------

int
StatsCollectorNode::encodeValue(const Position& pos, int value) const {
    assert(false);
    return 0;
}

double
StatsCollectorNode::entropy() const {
    assert(false);
    return 0.0;
}

std::unique_ptr<StatsNode>
StatsCollectorNode::getStats() const {
    return getBest()->getStats();
}

void
StatsCollectorNode::accept(Visitor& visitor, std::unique_ptr<Node>& owner) {
    visitor.visit(*this, owner);
}

std::string
StatsCollectorNode::describe(int indentLevel) const {
    return getBest()->describe(indentLevel);
}

// ------------------------------------------------------------

double
EncoderNode::entropy() const {
    assert(false);
    return 0.0;
}

bool
EncoderNode::applyData(const Position& pos, int value) {
    assert(false);
    return false;
}

void
EncoderNode::accept(Visitor& visitor, std::unique_ptr<Node>& owner) {
    visitor.visit(*this, owner);
}


}
