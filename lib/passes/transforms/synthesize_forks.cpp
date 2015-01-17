#include "synthesize_forks.hpp"

#include <llpm/module.hpp>
#include <libraries/synthesis/fork.hpp>
#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>


using namespace std;

namespace llpm {

void SynthesizeForksPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    if (mod->is<ControlRegion>())
        // Don't add forks to CRs
        return;

    deque<OutputPort*> forkingSources;
    for (const auto& p: conns->sinksRaw()) {
        if (p.second.size() > 1)
            forkingSources.push_back(p.first);
    }

    unsigned realForks = 0;
    for (auto op: forkingSources) {
        set<InputPort*> sinks;
        conns->findSinks(op, sinks);
        assert(sinks.size() > 1);

        bool virt = false;
        // Blocks contained in control regions don't require actual
        // forks since they share valid and backpressure signals
        if (mod->is<ControlRegion>())
            virt = true;

        auto fork = new Fork(op->type(), virt);
        if (!virt)
            realForks++;
        conns->connect(op, fork->din());
        for(auto sink: sinks) {
            conns->disconnect(op, sink);
            conns->connect(fork->createOutput(), sink);
        }
    }

    if (realForks > 0)
        printf("Created forks for %u forking sources\n", realForks);
}

} // namespace llpm
