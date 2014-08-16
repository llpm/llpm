#ifndef __LLPM_LLVM_OBJECTS_HPP__
#define __LLPM_LLVM_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMFunction: public ContainerModule {
    friend class LLVMTranslator;

    LLVMFunction(Design&, llvm::Function*);
    void build(llvm::Function* func);

public:
    virtual ~LLVMFunction();
};

} // namespace llpm

#endif // __LLPM_LLVM_OBJECTS_HPP__
