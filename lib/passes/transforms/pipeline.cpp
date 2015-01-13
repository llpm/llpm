#include "pipeline.hpp"

#include <llpm/connection.hpp>
#include <llpm/module.hpp>
#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <util/transform.hpp>
#include <analysis/graph_queries.hpp>

using namespace std;

namespace llpm {

void PipelineDependentsPass::runInternal(Module* mod) {
    if (mod->is<ControlRegion>())
        // Output rules do not apply within control regions.
        return;

    Transformer t(mod); 
    if (!t.canMutate())
        return;

    set<Block*> blocks;
    t.conns()->findAllBlocks(blocks);
    for (auto block: blocks) {
        unsigned numNormalOutputs = block->outputs().size();
        if (block->outputsSeparate() ||
            numNormalOutputs <= 1)
            continue;

        if (block->is<Split>()) {
            // if this block is a "split" we can resolve the non-tied
            // inputs by replacing it with some extracts instead.
            if (block->as<Split>()->refineToExtracts(*t.conns()))
                continue;
        }

        // Since this block has dependent outputs, all of them better
        // be connected to pipeline regs!
        for (auto op: block->outputs()) {
            set<InputPort*> sinks;
            PipelineRegister* preg = NULL;
            t.conns()->findSinks(op, sinks);
            for (auto sink: sinks)
                preg = sink->owner()->as<PipelineRegister>();
            if (preg == NULL) {
                preg = new PipelineRegister(op);
                t.conns()->connect(op, preg->din());
            }

            for (auto sink: sinks) {
                if (sink->owner()->isnot<PipelineRegister>()) {
                    t.conns()->disconnect(op, sink);
                    t.conns()->connect(preg->dout(), sink);
                }
            }
        }
    }
}

static bool isPipelineReg(Block* b) {
    return b->is<PipelineRegister>();
}

void PipelineCyclesPass::runInternal(Module* mod) {
    if (mod->is<ControlRegion>())
        // Never insert registers within a CR
        return;

    Transformer t(mod); 
    if (!t.canMutate())
        return;

    unsigned count = 0;

    // Strictly speaking, pipeline registers are only necessary for
    // _correctness_ when there exists a cycle in the graph. Therefore,
    // the _minimum_ pipelining is found by locating graph cycles and breaking
    // them with pipeline regs. We can further reduce pipeline regs by
    // breaking them starting with the most common edges in cycles --
    // breaking those edges simultaneously breaks the most cycles!
    //
    vector<Connection> cycle;
    while (queries::FindCycle(mod, &isPipelineReg, cycle)) {
        Connection toBreak = cycle.back();
        auto preg = new PipelineRegister(toBreak.source());
        t.insertBetween(toBreak, preg);
        // printf("    preg at: %s -> %s\n",
               // toBreak.source()->owner()->globalName().c_str(),
               // toBreak.sink()->owner()->globalName().c_str());
        count++;
    }

    printf("    Inserted %u pipeline registers\n", count);
}

} // namespace llpm

