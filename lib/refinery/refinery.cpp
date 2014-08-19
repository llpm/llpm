#include "refinery.hpp"

#include <llpm/module.hpp>

namespace llpm {

bool BlockRefiner::refine(Block* c) const {
    assert(false && "Not yet implemented");
}

bool BlockDefaultRefiner::handles(Block*) const {
    return true;
}

bool BlockDefaultRefiner::refine(
        const Block* block,
        std::vector<Block*>& blocks,
        ConnectionDB& conns) const {
    return block->refine(blocks, conns);
}

} // namespace llpm;

