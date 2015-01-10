#include "synthesize_forks.hpp"

#include <llpm/module.hpp>
#include <libraries/synthesis/fork.hpp>
#include <llpm/control_region.hpp>


using namespace std;

namespace llpm {

void SynthesizeForksPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    printf("Building forks for %s\n", mod->name().c_str());

    deque<OutputPort*> forkingSources;
    for (const auto& p: conns->sinksRaw()) {
        printf("  %s %lu\n",
               mod->design().namer().getName(p.first, mod).c_str(),
               p.second.size());
        if (p.second.size() > 1)
            forkingSources.push_back(p.first);
    }

    unsigned realForks = 0;
    for (auto op: forkingSources) {
        set<InputPort*> sinks;
        conns->findSinks(op, sinks);
        assert(sinks.size() > 1);

        bool virt = false;

        // Non-pipelineable links cannot have their LI pipelined
        // either
        if (!op->pipelineable())
            virt = true;

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
