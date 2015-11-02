#include "synthesize_forks.hpp"

#include <llpm/module.hpp>
#include <libraries/synthesis/fork.hpp>
#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <analysis/graph_queries.hpp>


using namespace std;

namespace llpm {

static bool isPipelineReg(Block* b) {
    return b->is<PipelineRegister>();
}

template<typename T>
unsigned intersection_size(const set<T>& a, const set<T>& b) {
    unsigned count;
    auto aIter = a.begin();
    auto bIter = b.begin();
    while (aIter != a.end() &&
           bIter != b.end()) {
        if (*aIter < *bIter) ++aIter;
        else if (*bIter < *aIter) ++bIter;
        else {
            ++count;
            ++aIter;
            ++bIter;
        }
    }
    return count;
}

void SynthesizeForksPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    if (mod->is<ControlRegion>())
        // Don't add forks to CRs
        return;

    set<Port*> constPorts;
    set<Block*> constBlocks;
    queries::FindConstants(mod, constPorts, constBlocks);

    deque<OutputPort*> forkingSources;
    for (const auto& p: conns->sinksRaw()) {
        if (p.second.size() > 1)
            forkingSources.push_back(p.first);
    }

    set<Fork*> realForks;
    for (auto op: forkingSources) {
        set<InputPort*> sinks;
        conns->findSinks(op, sinks);
        assert(sinks.size() > 1);
        
        bool virt = false;

        if (constPorts.count(op) > 0)
            // Don't need to actually fork const values
            virt = true;

        // Blocks contained in control regions don't require actual
        // forks since they share valid and backpressure signals
        if (mod->is<ControlRegion>())
            virt = true;

        auto fork = new Fork(op->type(), virt);
        if (!virt)
            realForks.insert(fork);
        if (!virt && _pipeline) {
            auto preg = new PipelineRegister(op);
            conns->connect(op, preg->din());
            conns->connect(preg->dout(), fork->din());
        } else {
            conns->connect(op, fork->din());
        }
        for(auto sink: sinks) {
            conns->disconnect(op, sink);
            conns->connect(fork->createOutput(), sink);
        }
    }

    if (realForks.size() > 0)
        printf("Created forks for %lu forking sources\n", realForks.size());

    // We now need to check if the fork outputs ever re-combine without
    // touching a pipelineregister. If they do, than those outputs need
    // pipeline regsiters or deadlock could occur.
    unsigned recombinedForks = 0;
    for (auto fork: realForks) {
        vector<set<InputPort*>> consumers;
        for (auto op: fork->outputs()) {
            consumers.push_back({});
            queries::FindConsumers(mod, op, consumers.back(), &isPipelineReg);
        }

        set<unsigned> pipelineTheseOutputs;
        for (unsigned i=0; i<fork->outputs().size(); i++) {
            for (unsigned j=i+1; j<fork->outputs().size(); j++) {
                const auto& cons1 = consumers[i];
                const auto& cons2 = consumers[j];
                if (intersection_size(cons1, cons2) > 0) {
                    pipelineTheseOutputs.insert(i);
                    pipelineTheseOutputs.insert(j);
                }
            }
        }

        for (auto opNum: pipelineTheseOutputs) {
            auto op = fork->outputs()[opNum];
            auto preg = new PipelineRegister(op);
            vector<InputPort*> sinks;
            conns->findSinks(op, sinks);
            for (auto s: sinks) {
                conns->disconnect(op, s);
                conns->connect(preg->dout(), s);
            }
            conns->connect(op, preg->din());
        }

        if (pipelineTheseOutputs.size() > 0) {
            recombinedForks++;
        }
    }
    printf("Found and pipelined %u recombining forks!\n", recombinedForks);
}

} // namespace llpm
