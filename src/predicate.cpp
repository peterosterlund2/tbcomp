#include "predicate.hpp"


std::unique_ptr<SinglePredicate>
SinglePredicate::getBest(std::unique_ptr<SinglePredicate>&& oldBest) const {
    if (oldBest && oldBest->entropy() <= entropy())
        return std::move(oldBest);
    return clone();
}
