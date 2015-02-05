#include "pipeline.hpp"

#include <llpm/connection.hpp>
#include <llpm/module.hpp>
#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <util/transform.hpp>
#include <util/llvm_type.hpp>
#include <analysis/graph.hpp>
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

struct FlowVisitor : public Visitor<OIEdge> {
    struct BlockStats {
        float flow;
        unsigned visits;
        unsigned outputEdges;
        unsigned inputEdges;

        BlockStats() :
            flow(0.0),
            visits(0),
            outputEdges(0),
            inputEdges(0)
        { }
    };

    map<Block*, BlockStats> flowAmt;

    float edgeFlow(const OutputPort* op) {
        Block* srcBlock = op->owner();
        BlockStats& bs = flowAmt[srcBlock];
        if (bs.outputEdges == 0)
            return -1.0;
        // printf("flow: %f\n", bs.flow);
        return bs.flow / bs.outputEdges;
    }

    Terminate visit(const ConnectionDB*, const OIEdge& edge) {
        Block* dstBlock = edge.endPort()->owner();
        if (dstBlock->is<PipelineRegister>())
            return TerminatePath;

        float ef = edgeFlow(edge.end().first);
        BlockStats& bs = flowAmt[dstBlock];
#if 0
        printf("  %p <- %p %u/%u\n",
               dstBlock, edge.end().first->owner(),
               bs.visits, bs.inputEdges);
#endif
        bs.flow += ef;
        bs.visits += 1;
        // if (bs.visits < bs.inputEdges)
            // return TerminatePath;
        return Continue;
    }

    void run(Module* mod) {
        ConnectionDB* conns = mod->conns();
        if (conns == NULL)
            return;

        vector<OutputPort*> initVec;
        mod->internalDrivers(initVec);

        set<OutputPort*> init(initVec.begin(), initVec.end());

        for (const Connection& c: *conns) {
            if (!c.sink()->owner()->is<PipelineRegister>()) {
                flowAmt[c.source()->owner()].outputEdges += 1;
                flowAmt[c.sink()->owner()].inputEdges += 1;
            }
            if (c.source()->owner()->is<PipelineRegister>() ||
                c.source()->owner()->inputs().size() == 0) {
                init.insert(c.source());
            }
        }

        for (auto op: init) {
            if (op->owner()->is<PipelineRegister>() ||
                op->owner()->inputs().size() == 0) {
                flowAmt[op->owner()].flow = 0.0;
            } else {
                flowAmt[op->owner()].flow = 1.0;
            }
        }

        GraphSearch<FlowVisitor, BFS> search(conns, *this);
        for (unsigned i=0; i<2; i++) {
            search.go(init);
        }
    }
};

void PipelineCyclesPass::runInternal(Module* mod) {
    if (mod->is<ControlRegion>())
        // Never insert registers within a CR
        return;

    Transformer t(mod); 
    if (!t.canMutate())
        return;

    unsigned count = 0;
    unsigned bits = 0;

    // Strictly speaking, pipeline registers are only necessary for
    // _correctness_ when there exists a cycle in the graph. Therefore,
    // the _minimum_ pipelining is found by locating graph cycles and breaking
    // them with pipeline regs. We can further reduce pipeline regs by
    // breaking them starting with the most common edges in cycles --
    // breaking those edges simultaneously breaks the most cycles!
    //
    vector<Connection> cycle;
    while (queries::FindCycle(mod, &isPipelineReg, cycle)) {
        FlowVisitor fvisitor;
        fvisitor.run(mod);

        Connection toBreak = cycle[rand() % cycle.size()];
        float maxFlow = 0;
        for (Connection c: cycle) {
            float flow = fvisitor.edgeFlow(c.source());
            if (flow > maxFlow) {
                maxFlow = flow;
                toBreak = c;
            }
        }

        auto preg = new PipelineRegister(toBreak.source());
        t.insertAfter(toBreak.source(), preg);
        // printf("    preg at: %s -> %s\n",
               // toBreak.source()->owner()->globalName().c_str(),
               // toBreak.sink()->owner()->globalName().c_str());
        count++;
        bits += bitwidth(preg->dout()->type());
    }

    printf("    Inserted %u pipeline registers (%u bits)\n", count, bits);
}

void LatchUntiedOutputs::runInternal(Module* mod) {
    if (mod->is<ControlRegion>())
        // Never insert registers within a CR
        return;

    Transformer t(mod); 
    if (!t.canMutate())
        return;

    unsigned count = 0;
    set<Block*> blocks;
    t.conns()->findAllBlocks(blocks);
    for (auto b: blocks) {
        if (b->outputsTied())
            continue;

        // If the outputs aren't tied, a latch may be necessary
        for (auto op: b->outputs()) {
            auto l = new Latch(op);
            t.insertAfter(op, l);
            count++;
        }
    }
    printf("    Inserted %u latches\n", count);
}

struct DelayVisitor : public Visitor<OIEdge> {
    struct Stats {
        Time inputPathDelay;
        unsigned visits;

        Stats() :
            inputPathDelay(),
            visits(0)
        { }
    };

    Backend* backend;
    Time period;
    map<OutputPort*, Stats> delays;
    set<OutputPort*> pipeline;

    DelayVisitor(Backend* b, Time period) :
        backend(b),
        period(period)
    { }

    Time edgeDelay(OutputPort* op) {
        if (pipeline.count(op) > 0)
            return Time();
        Stats& s = delays[op];
        return s.inputPathDelay + backend->maxLatency(op);
    }

    Terminate visit(const ConnectionDB*, const OIEdge& edge) {
        // Get the delay to the output port of our origin
        Time delay = edgeDelay(edge.end().first);

        // Add the routing delay
        delay += backend->latency(edge.end());

        if (delay >= period) {
            pipeline.insert(edge.end().first);
            delay = Time();
        }

        set<OutputPort*> deps;
        edge.endPort()->owner()->deps(edge.endPort(), deps);

        for (auto op: deps) {
            Stats& s = delays[op];
            if (s.inputPathDelay < delay)
                s.inputPathDelay = delay;
            s.visits += 1;
        }
        return Continue;
    }

    void run(Module* mod) {
        ConnectionDB* conns = mod->conns();
        if (conns == NULL)
            return;

        vector<OutputPort*> init;
        mod->internalDrivers(init);

        for (auto op: init) {
            if (op->owner()->is<PipelineRegister>()) {
                pipeline.insert(op->owner()->as<PipelineRegister>()->dout());
            }
        }

        GraphSearch<DelayVisitor, BFS> search(conns, *this);
        search.go(init);
    }
};

void PipelineFrequencyPass::runInternal(Module* mod) {
    // TODO: We assume a delay of 0.0 at the start of the module. That is
    // wrong. Get a delay from a previous execution of this function. Better
    // yet, run this function recursively with that number.

    DelayVisitor dv(_design.backend(), _maxDelay);
    dv.run(mod);
    unsigned bits = 0;
    for (auto p: dv.pipeline) {
        bits += bitwidth(p->type());
    }
    if (dv.pipeline.size() > 0) {
        printf("Inserting %lu pipeline registers (%u bits) "
               "into %s to meet timing...\n",
               dv.pipeline.size(),
               bits,
               mod->name().c_str());
    }

    set<Port*> constPorts;
    set<Block*> constBlocks;
    queries::FindConstants(mod, constPorts, constBlocks);

    Transformer t(mod);
    for (auto op: dv.pipeline) {
        if (op->owner()->is<PipelineRegister>())
            continue;
        if (constPorts.count(op) > 0)
            // Don't pipeline constants
            continue;
        if (t.conns()->countSinks(op) == 0)
            // Don't pipeline things with no consumer
            continue;
        auto preg = new PipelineRegister(op);
        t.insertAfter(op, preg);
    }

    if (mod->is<ControlRegion>() && dv.pipeline.size() > 0) {
        printf("    CR cycle latency: %u\n",
               mod->as<ControlRegion>()->clocks());
    }
}

} // namespace llpm

