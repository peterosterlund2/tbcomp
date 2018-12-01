#include "dtnode.hpp"
#include "predicate.hpp"
#include <cassert>
#include <string>
#include <iomanip>
#include <sstream>


namespace DT {

PredicateNode::PredicateNode()
  : Node(NodeType::PREDICATE) {
}

double
PredicateNode::cost() const {
    return left->cost() + right->cost();
}

std::unique_ptr<StatsNode>
PredicateNode::getStats() const {
    std::unique_ptr<StatsNode> s1 = left->getStats();
    std::unique_ptr<StatsNode> s2 = right->getStats();
    s1->addStats(s2.get());
    return std::move(s1);
}

std::string
PredicateNode::describe(int indentLevel) const {
    std::unique_ptr<StatsNode> s1 = left->getStats();
    std::unique_ptr<StatsNode> s2 = right->getStats();
    double costAfterPred = s1->cost() + s2->cost();

    s1->addStats(s2.get());

    std::stringstream ss;
    if (s1->isEmpty()) {
        ss << std::string(indentLevel*2, ' ');
        ss << pred->name() << '\n';
    } else {
        std::string statsStr = s1->describe(indentLevel);
        ss << statsStr.substr(0, statsStr.length() - 1) << ' ';

        std::stringstream ss2;
        ss2 << std::scientific << std::setprecision(2) << costAfterPred;
        ss << pred->name() << ' ' << ss2.str() << '\n';
    }

    ss << left->describe(indentLevel + 1);
    ss << right->describe(indentLevel + 1);
    return ss.str();
}

// ------------------------------------------------------------

double
StatsCollectorNode::cost() const {
    return getBest()->cost();
}

std::unique_ptr<StatsNode>
StatsCollectorNode::getStats() const {
    return getBest()->getStats();
}

std::string
StatsCollectorNode::describe(int indentLevel) const {
    return getBest()->describe(indentLevel);
}

// ------------------------------------------------------------

double
EncoderNode::cost() const {
    assert(false);
    return 0.0;
}

}
