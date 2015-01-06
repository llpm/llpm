#include "checks.hpp"

#include <llpm/module.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>

using namespace std;

namespace llpm {

void CheckConnectionsPass::runInternal(Module* m) {
    ConnectionDB* conns = m->conns();
    if (conns == NULL)
        return;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    auto& namer = m->design().namer();
    bool allGood = true;
    for (auto block: blocks) {
        for (auto ip: block->inputs()) {
            if (conns->findSource(ip) == NULL) {
                allGood = false;
                fprintf(stderr,
                        "WARNING: could not find driver for %s: %s %s\n",
                        namer.getName(ip, m).c_str(),
                        cpp_demangle(typeid(*block).name()).c_str(),
                        typestr(ip->type()).c_str());

                fprintf(stderr, "History:\n");
                block->history().print();
            }
        }
    }

    if (allGood)
        printf("No bad connections found in %s.\n", m->name().c_str());
}

};
