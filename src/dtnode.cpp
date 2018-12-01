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
PredicateNode::cost(const DT::EvalContext& ctx) const {
    return left->cost(ctx) + right->cost(ctx);
}

std::unique_ptr<StatsNode>
PredicateNode::getStats(const DT::EvalContext& ctx) const {
    std::unique_ptr<StatsNode> s1 = left->getStats(ctx);
    std::unique_ptr<StatsNode> s2 = right->getStats(ctx);
    s1->addStats(s2.get());
    return std::move(s1);
}

std::string
PredicateNode::describe(int indentLevel, const DT::EvalContext& ctx) const {
    std::unique_ptr<StatsNode> s1 = left->getStats(ctx);
    std::unique_ptr<StatsNode> s2 = right->getStats(ctx);
    double costAfterPred = s1->cost(ctx) + s2->cost(ctx);

    s1->addStats(s2.get());

    std::stringstream ss;
    if (s1->isEmpty()) {
        ss << std::string(indentLevel*2, ' ');
        ss << pred->name() << '\n';
    } else {
        std::string statsStr = s1->describe(indentLevel, ctx);
        ss << statsStr.substr(0, statsStr.length() - 1) << ' ';

        std::stringstream ss2;
        ss2 << std::scientific << std::setprecision(2) << costAfterPred;
        ss << pred->name() << ' ' << ss2.str() << '\n';
    }

    ss << left->describe(indentLevel + 1, ctx);
    ss << right->describe(indentLevel + 1, ctx);
    return ss.str();
}

// ------------------------------------------------------------

double
StatsCollectorNode::cost(const DT::EvalContext& ctx) const {
    return getBest(ctx)->cost(ctx);
}

std::unique_ptr<StatsNode>
StatsCollectorNode::getStats(const DT::EvalContext& ctx) const {
    return getBest(ctx)->getStats(ctx);
}

std::string
StatsCollectorNode::describe(int indentLevel, const DT::EvalContext& ctx) const {
    return getBest(ctx)->describe(indentLevel, ctx);
}

// ------------------------------------------------------------

double
EncoderNode::cost(const DT::EvalContext& ctx) const {
    assert(false);
    return 0.0;
}

}
