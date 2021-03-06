#include "checks.hpp"

#include <llpm/module.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>
#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <analysis/graph_queries.hpp>
#include <libraries/core/logic_intr.hpp>

using namespace std;

namespace llpm {

void CheckConnectionsPass::runInternal(Module* m) {
    ConnectionDB* conns = m->conns();
    if (conns == NULL)
        return;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    auto& namer = m->design().namer();
    // bool allGood = true;
    for (auto block: blocks) {
        for (auto ip: block->inputs()) {
            if (conns->findSource(ip) == NULL) {
                // allGood = false;
                fprintf(stderr,
                        "WARNING: could not find driver for %s: %s %s\n",
                        namer.getName(ip, m).c_str(),
                        cpp_demangle(typeid(*block).name()).c_str(),
                        typestr(ip->type()).c_str());

                fprintf(stderr, "History:\n");
                block->history().print();
                auto never = new Never(ip->type());
                conns->connect(never->dout(), ip);
            }
        }
    }

    // if (allGood)
        // printf("No bad connections found in %s.\n", m->name().c_str());
}

void CheckOutputsPass::runInternal(Module* m) {
    if (m->is<ControlRegion>())
        // Output rules do not apply within control regions.
        return;

    ConnectionDB* conns = m->conns();
    if (conns == NULL)
        return;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    auto& namer = m->design().namer();
    bool printedHeader = false;
    for (auto block: blocks) {
        if (block->outputsSeparate() ||
            block->outputs().size() <= 1)
            continue;

        // Since this block has dependent outputs, all of them better
        // be connected to pipeline regs!
        for (auto op: block->outputs()) {
            set<InputPort*> sinks;
            conns->findSinks(op, sinks);
            for (auto sink: sinks) {
                if (sink->owner()->isnot<PipelineRegister>()) {
                    if (!printedHeader) {
                        printf("ERROR: found un-pipelined connection from "
                               "block with dependent outputs:\n");
                        printedHeader = true;
                    }
                    printf("    %s -> %s\n",
                           namer.getName(op, m).c_str(),
                           namer.getName(sink, m).c_str());
                }
            }
        }
    }
}

static bool isPipelineReg(Block* b) {
    return b->is<PipelineRegister>();
}

void CheckCyclesPass::runInternal(Module* m) {
    std::vector< std::pair<const OutputPort*, const InputPort*> > cycle;
    auto found = queries::FindCycle(m, &isPipelineReg, cycle);
    if (found) {
        auto& namer = m->design().namer();
        printf("Error: found combinatorial loop in %s!\n",
               m->name().c_str());
        for (auto c: cycle) {
            printf("    %s -> %s\n",
                    namer.getName(c.first, m).c_str(),
                    namer.getName(c.second, m).c_str());
        }
    }
}

};
