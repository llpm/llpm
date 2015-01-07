#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llpm/llpm.hpp>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <refinery/refinery.hpp>
#include <synthesis/object_namer.hpp>
#include <backends/backend.hpp>
#include <util/macros.hpp>
#include <passes/manager.hpp>
#include <vector>

namespace llpm {

// Fwd defs. Are modern compilers really still 1-pass?
class Module;

class Design {
    llvm::LLVMContext& _context;
    Refinery* _refinery;
    std::vector<Module*> _modules;
    ObjectNamer* _namer;
    Backend* _backend;

    PassManager _elaborations;
    PassManager _optimizations;

    std::deque<std::unique_ptr<llvm::Module>> _llvmModules;

public:
    static llvm::LLVMContext& Default_LLVMContext;

    Design(llvm::LLVMContext& ctxt = Default_LLVMContext) :
        _context(ctxt),
        _refinery(new Refinery()),
        _namer(NULL),
        _elaborations(*this),
        _optimizations(*this)
    {
        _refinery->refiners().appendEntry(new BlockDefaultRefiner());
    }

    Refinery& refinery() {
        return *_refinery;
    }

    DEF_GET_NP(backend);
    DEF_SET_NONULL(backend);
    DEF_GET(elaborations);
    DEF_GET(optimizations);

    void elaborate();
    void optimize();

    void refine(Module* m);

    const std::vector<Module*>& modules() const {
        return _modules;
    }

    void addModule(Module* module) {
        _modules.push_back(module);
    }

    llvm::LLVMContext& context() const {
        return _context;
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
};

} // namespace llpm

#endif // __LLPM_DESIGN_HPP__
