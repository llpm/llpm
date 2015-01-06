#include "history.hpp"

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <util/misc.hpp>

using namespace std;

namespace llpm {

static void printTabs(unsigned tabs) {
    for (unsigned i=0; i<tabs; i++) {
        printf("    ");
    }
}

void BlockHistory::print(unsigned tabs) const {
    const char* source;
    switch(_src) {
    case Unknown:
        source = "unknown"; break;
    case Frontend:
        source = "frontend"; break;
    case Refinement:
        source = "refinement"; break;
    case Optimization:
        source = "optimization"; break;
    default:
        source = "???"; break;
    }
    printTabs(tabs); printf("Source: %s %s\n",
                            source, _meta.c_str());
    for (Block* b: _srcBlocks) {
        auto name = b->module()->design().namer().getName(b, b->module());
        printTabs(tabs); printf("%s (%s)",
                                name.c_str(),
                                cpp_demangle(typeid(*b).name()).c_str());
        b->history().print(tabs+1);
    }
}

} // namespace
