#include "refinery.hpp"

#include <llpm/module.hpp>

namespace llpm {

bool BlockRefiner::refine(Block* c, std::vector<Block*>& blocks) const {
    std::map<InputPort*, vector<InputPort*> > ipMap;
    std::map<OutputPort*, OutputPort*> opMap;

    auto rc = this->refine(c, blocks, ipMap, opMap);
    if (!rc)
        return false;

    Module* m = c->module();
    ConnectionDB* conns = m->conns();
    if (conns == NULL)
        throw IncompatibleException(
            "BlockRefiner cannot conduct a modifying" \
            "refine on a module which does not expose its connections");

    assert(false && "Not implemented yet");
    return rc;
}

bool BlockDefaultRefiner::handles(Block*) const {
    return true;
}

bool BlockDefaultRefiner::refine(
        const Block* block,
        std::vector<Block*>& blocks,
        std::map<InputPort*, vector<InputPort*> >& ipMap,
        std::map<OutputPort*, OutputPort*>& opMap) const {
    return block->refine(blocks, ipMap, opMap);
}

} // namespace llpm;

