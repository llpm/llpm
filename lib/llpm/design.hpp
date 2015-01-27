#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llpm/llpm.hpp>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <refinery/refinery.hpp>
#include <synthesis/object_namer.hpp>
#include <backends/backend.hpp>
#include <wedges/wedge.hpp>
#include <util/macros.hpp>
#include <util/files.hpp>
#include <passes/manager.hpp>

#include <memory>
#include <vector>
#include <boost/program_options.hpp>

namespace llvm {
    class PassRegistry;
};

namespace llpm {

// Fwd defs. Are modern compilers really still 1-pass?
class Module;
class GraphvizOutput;

class Design {
    std::shared_ptr<llvm::LLVMContext> _context;
    llvm::PassRegistry* _passReg;
    Refinery* _refinery;
    std::vector<Module*> _modules;
    ObjectNamer* _namer;
    Backend* _backend;
    Wedge* _wedge;
    Wrapper* _wrapper;
    GraphvizOutput* _gvOutput;
    FileSet _workingDir;

    PassManager _elaborations;
    PassManager _optimizations;

    std::set<llvm::Module*> _llvmModules;

    boost::program_options::options_description _optDesc;
    void buildOpts();

public:
    Design(std::shared_ptr<llvm::LLVMContext> ctxt = NULL) :
        _passReg(NULL),
        _refinery(new Refinery()),
        _namer(NULL),
        _backend(NULL),
        _wedge(NULL),
        _wrapper(NULL),
        _gvOutput(NULL),
        _workingDir(),
        _elaborations(*this, "elab"),
        _optimizations(*this, "opt"),
        _optDesc("Design global options")
    {
        if (ctxt == NULL)
            ctxt = std::make_shared<llvm::LLVMContext>();
        _context = ctxt;
        _refinery->refiners().appendEntry(
            std::make_shared<BlockDefaultRefiner>());
        buildOpts();
    }

    ~Design();

    boost::program_options::options_description& optDesc() {
        return _optDesc;
    }
    void notify(boost::program_options::variables_map&);

    /**
     * Do all elaboration and synthesis
     */
    int go();

    Refinery& refinery() {
        return *_refinery;
    }

    DEF_GET_NP(backend);
    DEF_SET_NONULL(backend);
    DEF_GET_NP(wedge);
    DEF_SET_NONULL(wedge);
    DEF_GET_NP(wrapper);
    DEF_SET(wrapper);
    DEF_GET(elaborations);
    DEF_GET(optimizations);

    DEF_GET(workingDir);
    GraphvizOutput* gv();

    void elaborate(bool debug = false);
    void optimize(bool debug = false);

    void refine(Module* m);

    const std::vector<Module*>& modules() const {
        return _modules;
    }

    void addModule(Module* module) {
        _modules.push_back(module);
    }

    llvm::LLVMContext& context() const {
        return *_context;
    }

    ObjectNamer& namer() {
        if (_namer == NULL) {
            _namer = new ObjectNamer();
        }
        return *_namer;
    };

    // Reads in some bitcode, creating an LLVM module. Retains
    // ownership of said module since things may depend on it.
    llvm::Module* readBitcode(std::string fnName);
    void assumeOwnership(llvm::Module* mod) {
        _llvmModules.insert(mod);
    }
    void deleteModule(llvm::Module* mod) {
        _llvmModules.erase(mod);
        delete mod;
    }

    llvm::PassRegistry* llvmPassReg();
};

} // namespace llpm

#endif // __LLPM_DESIGN_HPP__
