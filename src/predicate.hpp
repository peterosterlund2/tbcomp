#ifndef PREDICATE_HPP_
#define PREDICATE_HPP_

#include "util/util.hpp"


class Position;
namespace DT {
    class EvalContext;
}

class Predicate {
public:
    virtual ~Predicate() = default;

    /** Return true if predicate is true for "pos". */
    virtual bool eval(const Position& pos, DT::EvalContext& ctx) const = 0;

    /** For debugging. */
    virtual std::string name() const = 0;
};


#endif /* PREDICATE_HPP_ */
