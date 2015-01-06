#include "design.hpp"

#include <llpm/module.hpp>
#include <util/misc.hpp>

using namespace std;

namespace llpm {

llvm::LLVMContext& Design::Default_LLVMContext = llvm::getGlobalContext();
static const unsigned MaxPasses = 100;


class SetFrontendHistory: public ModulePass {
protected:
    virtual void runInternal(Module* m) {
        ConnectionDB* conns = m->conns();
        if (conns == NULL)
            return;

        set<Block*> blocks;
        conns->findAllBlocks(blocks);
        for (auto b: blocks) {
            if (b->history().src() == BlockHistory::Unset)
                b->history().setFrontend();
        }
    }

public:
    SetFrontendHistory(Design& d) :
        ModulePass(d) { }
};

void setUnknownElaboration(Module* m) {
    std::set<Block*> blocks;
    m->conns()->findAllBlocks(blocks);
    for (Block* b: blocks) {
        if (b->history().src() == BlockHistory::Unset) {
            b->history().setUnknown();
            b->history().meta("elaboration");
        }
    }
}

void Design::elaborate() {
    auto sc = backend()->primitiveStops();

    // Set "frontend" as history for all Unset histories
    SetFrontendHistory sfh(*this);
    sfh.run();

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
        LambdaModulePass p(*this, setUnknownElaboration);
        p.run(m);
        m->validityCheck();

        if (!refined) {
            printf("Error: could not finish refining!\n");
            printf("Remaining blocks to be refined: \n");
            vector<Block*> blocks;
            m->blocks(blocks);
            sc->unrefined(blocks);
            for(Block* b: blocks) {
                printf("\t%s: %s\n",
                       cpp_demangle(typeid(*b).name()).c_str(),
                       b->print().c_str());
            }
        }
    }
}

void Design::optimize() {
    _optimizations.run();

    LambdaModulePass p(*this,
        [](Module* m) {
            std::set<Block*> blocks;
            m->conns()->findAllBlocks(blocks);
            for (Block* b: blocks) {
                if (b->history().src() == BlockHistory::Unset) {
                    b->history().setOptimization(NULL);
                    b->history().meta("(unknown which optimization)");
                }
            }
        });
    p.run();
}

} // namespace llpm
#include <util/macros.hpp>
