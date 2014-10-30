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
        ConnectionDB& conns) const {
    return block->refine(conns);
}

unsigned Refinery::refine(std::vector<Block*> crude,
                          ConnectionDB& conns,
                          StopCondition* sc) {
    unsigned passes = 0;
    bool foundRefinement;
    do {
        set<Block*> refinedBlocks;
        for(Block*& c: crude) {
            if (sc && sc->stopRefine(c))
                continue;
            const vector<Refiner*>& possible_refiners = _refiners(c);

            for(auto& r: possible_refiners) {
                conns.clearNewBlocks();
                if(r->refine(c, conns)) {
                    refinedBlocks.insert(c);
                    assert(conns.isUsed(c) == false);
                    std::set<Block*> newBlocks;
                    conns.readAndClearNewBlocks(newBlocks);
                    for (Block* nb: newBlocks) {
                        assert(nb != c);
                        assert(conns.isUsed(nb));
                        assert(nb->history().src() == BlockHistory::Unknown);
                        nb->history().setRefinement(c);
                    }
                    break;
                }
            }
        }

        if (refinedBlocks.size() > 0) {
            passes += 1;
            foundRefinement = true;
        } else {
            foundRefinement = false;
        }

        set<Block*> newCrude;
        conns.findAllBlocks(newCrude);
        crude = vector<Block*>(newCrude.begin(), newCrude.end());
        for (Block* b: newCrude) {
            if (refinedBlocks.count(b) > 0)
                throw ImplementationError("Refined block still present in connection DB!");
        }
        printf("Pass: %u done\n", passes);
    } while (foundRefinement);

    return passes;
}



} // namespace llpm;

