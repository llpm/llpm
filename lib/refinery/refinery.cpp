#include "refinery.hpp"

namespace llpm {

bool BlockDefaultRefiner::handles(std::type_index ti) const {
    return true;
}

bool BlockDefaultRefiner::refine(std::vector<Block*>& newCrude, Block* c) const {
    if (!c->refinable())
        return false;

    auto rc = c->refine(newCrude);
    assert(rc);
    return true;
}

} // namespace llpm;

