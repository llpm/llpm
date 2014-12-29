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
    for (auto block: blocks) {
        for (auto ip: block->inputs()) {
            if (conns->findSource(ip) == NULL)
                fprintf(stderr,
                        "WARNING: could not find driver for %s.%s: %s.%s\n",
                        block->name().c_str(),
                        ip->name().c_str(),
                        cpp_demangle(typeid(*block).name()).c_str(),
                        typestr(ip->type()).c_str());
        }
    }
}

};
