#include "design.hpp"

#include <llpm/module.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <util/misc.hpp>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

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

GraphvizOutput* Design::gv() {
    if (_gvOutput == NULL)
        _gvOutput = new GraphvizOutput(*this);
    return _gvOutput;
}

void Design::elaborate(bool debug) {
    auto sc = backend()->primitiveStops();

    // Set "frontend" as history for all Unset histories
    SetFrontendHistory sfh(*this);
    sfh.run();

    for (Module* m: _modules) {
        m->validityCheck();

        bool refined;
        unsigned passes;
        for (passes = 0; passes < MaxPasses; passes++) {
            bool changed = _elaborations.run(m, debug);
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

void Design::optimize(bool debug) {
    _optimizations.run(debug);

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


llvm::Module* Design::readBitcode(std::string fnName) {
    llvm::PassRegistry *Registry = llvm::PassRegistry::getPassRegistry();
    initializeCore(*Registry);
    initializeAnalysis(*Registry);
    // initializeBasicCallGraphPass(*Registry);
    initializeScalarOpts(*Registry);

    llvm::SMDiagnostic Err;
    llvm::Module *mod = 
        llvm::ParseIRFile(fnName, Err, context());
    _llvmModules.emplace_back(mod);
    
    if (mod == NULL) {
            Err.print("LLVMTranslator::readBitcode", llvm::errs());
            throw Exception("Could not parse module\n");
    }
    return mod;
}

} // namespace llpm
#include <util/macros.hpp>
