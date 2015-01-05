#include "synthesize_forks.hpp"

#include <llpm/module.hpp>
#include <libraries/synthesis/fork.hpp>


using namespace std;

namespace llpm {

void SynthesizeForksPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    deque<OutputPort*> forkingSources;
    for (const auto& p: conns->sinksRaw()) {
        if (p.second.size() > 1)
            forkingSources.push_back(p.first);
    }

    for (auto op: forkingSources) {
        set<InputPort*> sinks;
        conns->findSinks(op, sinks);
        assert(sinks.size() > 1);

        auto fork = new Fork(op->type());
        conns->connect(op, fork->din());
        for(auto sink: sinks) {
            conns->disconnect(op, sink);
            conns->connect(fork->createOutput(), sink);
        }
    }

    printf("Created forks for %lu forking sources\n", forkingSources.size());
}

} // namespace llpm
