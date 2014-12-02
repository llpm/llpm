#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llpm/llpm.hpp>
#include <llvm/IR/LLVMContext.h>
#include <refinery/refinery.hpp>
#include <synthesis/object_namer.hpp>
#include <backends/backend.hpp>
#include <util/macros.hpp>

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

public:
    static llvm::LLVMContext& Default_LLVMContext;

    Design(llvm::LLVMContext& ctxt = Default_LLVMContext) :
        _context(ctxt),
        _refinery(new Refinery()),
        _namer(NULL)
    {
        _refinery->refiners().appendEntry(new BlockDefaultRefiner());
    }

    Refinery& refinery() {
        return *_refinery;
    }

    DEF_GET_NP(backend);
    DEF_SET_NONULL(backend);

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
};

} // namespace llpm

#endif // __LLPM_DESIGN_HPP__
