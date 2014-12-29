#include "design.hpp"

#include <llpm/module.hpp>

using namespace std;

namespace llpm {

llvm::LLVMContext& Design::Default_LLVMContext = llvm::getGlobalContext();
static const unsigned MaxPasses = 1000;

void Design::elaborate() {
    auto sc = backend()->primitiveStops();
    for (Module* m: _modules) {
        m->validityCheck();

        bool refined;
        unsigned passes;
        for (passes = 0; passes < MaxPasses; passes++) {
            bool changed = _elaborations.run(m);
            refined = m->refined(sc);
            if (refined || !changed)
                break;
        }

        printf("%u refinement passes on '%s'\n", passes, m->name().c_str());
        if (!refined) {
            printf("Error: could not finish refining!\n");
            printf("Remaining blocks to be refined: \n");
            vector<Block*> blocks;
            m->blocks(blocks);
            sc->unrefined(blocks);
            for(Block* b: blocks) {
                printf("\t%s\n", typeid(*b).name());
            }
        }

        m->validityCheck();
    }

}

void Design::optimize() {
    _optimizations.run();
}

} // namespace llpm
