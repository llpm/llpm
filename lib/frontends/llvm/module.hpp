#ifndef __LLPM_LLVM_MODULE_HPP__
#define __LLPM_LLVM_MODULE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMModule: public ContainerModule {
    friend class LLVMTranslator;

    LLVMModule(Design&, llvm::Function*);

public:
    virtual ~LLVMModule();
};

} // namespace llpm

#endif // __LLPM_LLVM_MODULE_HPP__
