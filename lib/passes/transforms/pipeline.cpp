#include "pipeline.hpp"

#include <llpm/connection.hpp>
#include <llpm/module.hpp>
#include <llpm/control_region.hpp>
#include <llpm/scheduled_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <libraries/synthesis/fork.hpp>
#include <util/transform.hpp>
#include <util/llvm_type.hpp>
#include <analysis/graph.hpp>
#include <analysis/graph_queries.hpp>

using namespace std;

namespace llpm {

void PipelineDependentsPass::runInternal(Module* mod) {
    if (mod->is<ControlRegion>() ||
        mod->is<ScheduledRegion>() )
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

        if (block->is<ScheduledRegion>()) {
            // ScheduledRegions do wacky things with their inputs and outputs.
            // Let them handle it.
            continue;
        }

        if (block->is<Split>()) {
            auto split = block->as<Split>();
            auto origInput = t.conns()->findSource(split->din());
            // if this block is a "split" we can resolve the non-tied
            // inputs by replacing it with some extracts instead.
            auto extracts = split->refineToExtracts(*t.conns());
            auto fork = new Fork(block->as<Split>()->din()->type(), false);
            t.conns()->connect(origInput, fork->din());
            for (auto e: extracts) {
                t.conns()->disconnect(origInput, e->din());
                t.conns()->connect(fork->createOutput(), e->din());
            }

            continue;
        }

        // Blocks cannot have dependent outputs for synthesis. To create a
        // single output, join the dependent outputs then fork it to the
        // consumers.
        vector<llvm::Type*> tyVec;
        for (auto op: block->outputs()) {
            tyVec.push_back(op->type());
        }
        auto join = new Join(tyVec);
        auto fork = new Fork(join->dout()->type(), false);
        t.conns()->connect(join->dout(), fork->din());
        unsigned i=0;
        for (auto op: block->outputs()) {
            set<InputPort*> sinks;
            t.conns()->findSinks(op, sinks);
            t.conns()->connect(op, join->din(i));
            auto extr = new Extract(join->dout()->type(), {i});
            t.conns()->connect(fork->createOutput(), extr->din());

            for (auto sink: sinks) {
                t.conns()->disconnect(op, sink);
                t.conns()->connect(extr->dout(), sink);
            }
            i++;
        }
    }
}

static bool breaksCycle(Block* b) {
    return b->is<PipelineRegister>() ||
           b->is<ScheduledRegion>();
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
    if (mod->is<ControlRegion>() ||
        mod->is<ScheduledRegion>() )
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
    std::vector< std::pair<const OutputPort*, const InputPort*> > cycle;
    while (queries::FindCycle(mod, &breaksCycle, cycle)) {
        FlowVisitor fvisitor;
        fvisitor.run(mod);

        auto toBreak = cycle[rand() % cycle.size()];
        float maxFlow = 0;
        for (auto c: cycle) {
            float flow = fvisitor.edgeFlow(c.first);
            if (flow > maxFlow) {
                maxFlow = flow;
                toBreak = c;
            }
        }

        auto preg = new PipelineRegister(toBreak.first);
        t.insertAfter((OutputPort*)toBreak.first, preg);
        // printf("    preg at: %s -> %s\n",
               // toBreak.source()->owner()->globalName().c_str(),
               // toBreak.sink()->owner()->globalName().c_str());
        count++;
        bits += bitwidth(preg->dout()->type());
    }

    printf("    Inserted %u pipeline registers (%u bits)\n", count, bits);
}

void LatchUntiedOutputs::runInternal(Module* mod) {
    if (mod->is<ControlRegion>() ||
        mod->is<ScheduledRegion>() )
        // Never insert registers within a CR
        return;

    Transformer t(mod); 
    if (!t.canMutate())
        return;

    unsigned count = 0;
    set<Block*> blocks;
    t.conns()->findAllBlocks(blocks);
    for (auto b: blocks) {
        if (b->outputsTied() || 
            b->outputs().size() <= 1 ||
            b->outputsSeparate() )
            continue;

        // If the outputs aren't tied, a latch may be necessary
        for (auto op: b->outputs()) {
            if (!_useRegs) {
                auto l = new Latch(op);
                t.insertAfter(op, l);
            } else {
                auto p = new PipelineRegister(op);
                t.insertAfter(op, p);
            }
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

    PipelineFrequencyPass* pipePass;
    Backend* backend;
    Time period;
    map<const OutputPort*, Stats> delays;
    set<const OutputPort*> pipeline;
    set<const Port*> constPorts;
    set<Block*> constBlocks;

    DelayVisitor(PipelineFrequencyPass* pass, Backend* b, Time period) :
        pipePass(pass),
        backend(b),
        period(period)
    { }

    Time edgeDelay(const OutputPort* op) {
        if (pipeline.count(op) > 0 || constPorts.count(op) > 0)
            return Time();
        Stats& s = delays[op];
        if (op->owner()->is<MutableModule>()) {
            auto f = pipePass->_modOutDelays.find(op);
            if (f == pipePass->_modOutDelays.end()) {
                pipePass->runOnModule(op->owner()->as<Module>(), s.inputPathDelay);
            }
            return pipePass->_modOutDelays[op];
        } else {
            return s.inputPathDelay + backend->maxTime(op).time();
        }
    }

    Terminate visit(const ConnectionDB*, const OIEdge& edge) {
        // Get the delay to the output port of our origin
        Time delay = edgeDelay(edge.end().first);

        // Add the routing delay
        delay += backend->latency(edge.end().second, edge.end().first).time();

        if (delay >= period) {
            pipeline.insert(edge.end().first);
            delay = Time();
        }

        set<const OutputPort*> deps;
        edge.endPort()->owner()->deps(edge.endPort(), deps);

        for (auto op: deps) {
            Stats& s = delays[op];
            if (s.inputPathDelay < delay)
                s.inputPathDelay = delay;
            s.visits += 1;
        }
        return Continue;
    }

    void run(Module* mod, Time initTime) {
        ConnectionDB* conns = mod->conns();
        if (conns == NULL)
            return;

        queries::FindConstants(mod, constPorts, constBlocks);

        vector<OutputPort*> init;
        mod->internalDrivers(init);

        for (auto op: init) {
            delays[op].inputPathDelay = initTime;
        }

        set<Block*> blocks;
        for (auto block: blocks) {
            if (block->is<PipelineRegister>()) {
                pipeline.insert(block->as<PipelineRegister>()->dout());
            }
        }

        GraphSearch<DelayVisitor, BFS> search(conns, *this);
        search.go(init);
    }
};

bool PipelineFrequencyPass::runOnModule(Module* mod, Time initTime) {
    // TODO: We assume a delay of 0.0 at the start of the module. That is
    // wrong. Get a delay from a previous execution of this function. Better
    // yet, run this function recursively with that number.

    DelayVisitor dv(this, _design.backend(), _maxDelay);
    dv.run(mod, initTime);
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

    Transformer t(mod);
    for (auto op: dv.pipeline) {
        if (op->owner()->is<PipelineRegister>())
            continue;
        if (t.conns()->countSinks(op) == 0)
            // Don't pipeline things with no consumer
            continue;
        auto preg = new PipelineRegister(op);
        t.insertAfter((OutputPort*)op, preg);
    }

    if (mod->is<ControlRegion>() && dv.pipeline.size() > 0) {
        mod->as<ControlRegion>()->schedule();
        printf("    CR cycle latency: %u\n",
               mod->as<ControlRegion>()->clocks());
    }

    for (auto modOp: mod->outputs()) {
        OutputPort* intOp = t.conns()->findSource(mod->getSink(modOp));
        assert(intOp != nullptr);
        _modOutDelays[modOp] = dv.edgeDelay(intOp);
    }

    return dv.pipeline.size() > 0;
}

bool PipelineFrequencyPass::run() {
    bool ret = false;
    auto mods = _design.modules();
    for (Module* m: mods) {
        if (runOnModule(m, Time::s(0)))
            ret = true;
        m->validityCheck();
    }
    return ret;
}

} // namespace llpm

