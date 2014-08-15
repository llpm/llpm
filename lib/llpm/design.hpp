#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llvm/IR/LLVMContext.h>
#include <refinery/refinery.hpp>
#include <llpm/block.hpp>
#include <llpm/module.hpp>

#include <vector>

namespace llpm {

class Design {
    llvm::LLVMContext& _context;
    Refinery<Block>* _refinery;
    std::vector<Module*> _modules;

public:
    static llvm::LLVMContext& Default_LLVMContext;

    Design(llvm::LLVMContext& ctxt = Default_LLVMContext) :
        _context(ctxt),
        _refinery(new Refinery<Block>())
    {
        _refinery->refiners().appendEntry(new BlockDefaultRefiner());
    }

    Refinery<Block>& refinery() {
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
