#include "design.hpp"

#include <llpm/module.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

using namespace std;

namespace llpm {

Design::~Design() {
    for (auto m: _modules) {
        delete m;
    }
    DEL_IF(_refinery);
    DEL_IF(_namer);
    DEL_IF(_backend);
    DEL_IF(_gvOutput);

    for (auto m: _llvmModules) {
        delete m;
    }
    DEL_IF(_passReg);
}

int Design::go() {
    // If wedges require wrapper, wrap away!
    printf("Wrapping modules...\n");
    if (_wrapper) {
        for (unsigned i=0; i<_modules.size(); i++) {
            _modules[i] = _wrapper->wrapModule(_modules[i]);
        }
    }
      
    printf("Elaborating...\n");
    this->elaborate(true);
    printf("Optimizing...\n");
    this->optimize(true);

    FileSet* fs = workingDir();

    // printf("Writing graphviz output...\n");
    // for (Module* mod: d.modules()) {
        // gv.writeModule(fs.create(mod->name() + ".gv"), mod);
    // }

    for (Module* mod: modules()) {
        printf("Interfaces: \n");
        for (Interface* i: mod->interfaces()) {
            printf("  <-> %s  %s -> %s\n",
                   i->name().c_str(),
                   typestr(i->req()->type()).c_str(),
                   typestr(i->resp()->type()).c_str());
        }

        printf("Ports: \n");
        for (InputPort* ip: mod->inputs()) {
            printf("  -> %s : %s\n",
                   ip->name().c_str(), typestr(ip->type()).c_str());
        }
        for (OutputPort* op: mod->outputs()) {
            printf("  <- %s : %s\n",
                   op->name().c_str(), typestr(op->type()).c_str());
        }

        printf("Writing graphviz output...\n");
        auto f =fs->create(mod->name() + ".gv");
        gv()->writeModule(f, mod, true);
        f->close();
        f = fs->create(mod->name() + "_simple.gv");
        gv()->writeModule(f, mod, false);
        f->close();

        vector<Module*> submodules;
        mod->submodules(submodules);
        for (auto sm: submodules) {
            gv()->writeModule(fs->create(sm->name() + ".gv"), sm, true);
        }
        fs->flush();

        if (_wedge) {
            printf("Writing Verilog output...\n");
            _wedge->writeModule(*fs, mod);
        }
    }

    return 0;
}

static const unsigned MaxPasses = 100;

class SetFrontendHistory: public ModulePass {
protected:
    virtual void runInternal(Module* m) {
        const ConnectionDB* conns = m->connsConst();
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
    m->connsConst()->findAllBlocks(blocks);
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
            m->connsConst()->findAllBlocks(blocks);
            for (Block* b: blocks) {
                if (b->history().src() == BlockHistory::Unset) {
                    b->history().setOptimization(NULL);
                    b->history().meta("(unknown which optimization)");
                }
            }
        });
    p.run();
}

llvm::PassRegistry* Design::llvmPassReg() {
    if (_passReg == NULL) {
        _passReg = llvm::PassRegistry::getPassRegistry();
        initializeCore(*_passReg);
        initializeAnalysis(*_passReg);
        initializeScalarOpts(*_passReg);
        initializeTransformUtils(*_passReg);
        initializeVectorization(*_passReg);
        initializeInstCombine(*_passReg);
    }
    return _passReg;
}

llvm::Module* Design::readBitcode(std::string fnName) {
    llvmPassReg();
    llvm::SMDiagnostic Err;
    llvm::Module *mod = 
        llvm::parseIRFile(fnName, Err, context()).release();
    _llvmModules.insert(mod);
    
    if (mod == NULL) {
            Err.print("LLVMTranslator::readBitcode", llvm::errs());
            throw Exception("Could not parse module\n");
    }
    return mod;
}

} // namespace llpm
#include <util/macros.hpp>
