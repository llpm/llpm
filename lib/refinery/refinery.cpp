#include "refinery.hpp"

#include <llpm/module.hpp>

using namespace std;

namespace llpm {

bool BlockRefiner::refine(Block*) const {
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
                          int depth,
                          StopCondition* sc) {
    long passes = 0;
    bool foundRefinement;
    do {
        set<Block*> refinedBlocks;
        for(Block*& c: crude) {
            BlockP cShared = c->getptr();
            if (sc && sc->stopRefine(c))
                continue;

            // List of possible refiners
            const vector<Refiner*>& possible_refiners = _refiners(c);
            for(auto& r: possible_refiners) {
                conns.clearNewBlocks();
                auto changeCounter = conns.changeCounter();
                if(r->refine(c, conns)) {
                    // This refiner did the job!
                    refinedBlocks.insert(c);
                    if (conns.isUsed(c)) {
                        for (InputPort* ip: c->inputs()) {
                            OutputPort* op = conns.findSource(ip);
                            if (op != nullptr) {
                                fprintf(stderr, "Refinement error: Block %s input port %s is still driven by %s.%s!\n",
                                        c->name().c_str(), ip->name().c_str(),
                                        op->owner()->name().c_str(), op->name().c_str());
                            }
                        }
                        for (OutputPort* op: c->outputs()) {
                            std::set<InputPort*> sinks;
                            conns.findSinks(op, sinks);
                            for (InputPort* ip: sinks) { 
                                fprintf(stderr, "Refinement error: Block %s output port %s is still drives %s.%s!\n",
                                        c->name().c_str(), op->name().c_str(),
                                        ip->owner()->name().c_str(), ip->name().c_str());
                            }
                        }
                    }
                    assert(conns.isUsed(c) == false);
                    std::set<BlockP> newBlocks;
                    conns.readAndClearNewBlocks(newBlocks);
                    for (BlockP nb: newBlocks) {
                        assert(nb.get() != c);
                        assert(conns.isUsed(nb.get()));
                        if (nb->history().src() == BlockHistory::Unset)
                            nb->history().setRefinement(cShared);
                    }
                    break;
                } else {
                    // If the refiner didn't work, make sure it didn't
                    // touch anything! (Sanity check)
                    assert(changeCounter == conns.changeCounter());
                    set<BlockP> newBlocks;
                    conns.readAndClearNewBlocks(newBlocks);
                    assert(newBlocks.size() == 0); // Should be redundant
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
    } while (foundRefinement && (depth == -1 || passes < depth));

    return passes;
}



} // namespace llpm;

