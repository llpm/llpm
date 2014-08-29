#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llpm/llpm.hpp>
#include <llvm/IR/LLVMContext.h>
#include <refinery/refinery.hpp>

#include <vector>

namespace llpm {

// Fwd defs. Are modern compilers really still 1-pass?
class Module;

class Design {
public:
    typedef llpm::Refinery<Block> Refinery;

private:
    llvm::LLVMContext& _context;
    Refinery* _refinery;
    std::vector<Module*> _modules;

public:
    static llvm::LLVMContext& Default_LLVMContext;

    Design(llvm::LLVMContext& ctxt = Default_LLVMContext) :
        _context(ctxt),
        _refinery(new Refinery())
    {
        _refinery->refiners().appendEntry(new BlockDefaultRefiner());
    }

    Refinery& refinery() {
        return *_refinery;
    }

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
};

} // namespace llpm

#endif // __LLPM_DESIGN_HPP__
