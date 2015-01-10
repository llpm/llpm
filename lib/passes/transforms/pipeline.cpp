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
        if (block->outputsSeparate() ||
            block->outputs().size() <= 1)
            continue;

        // Since this block has dependent outputs, all of them better
        // be connected to pipeline regs!
        for (auto op: block->outputs()) {
            set<InputPort*> sinks;
            t.conns()->findSinks(op, sinks);
            for (auto sink: sinks) {
                if (sink->owner()->isnot<PipelineRegister>()) {
                    auto preg = new PipelineRegister(op);
                    t.insertBetween(Connection(op, sink), preg);
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
    }
}

} // namespace llpm

