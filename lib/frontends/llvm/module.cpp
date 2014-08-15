#include "module.hpp"

#include <llvm/IR/Function.h>

namespace llpm {

LLVMModule::LLVMModule(llpm::Design& design, llvm::Function* func) :
    ContainerModule(design, func->getName())
{
}

LLVMModule::~LLVMModule() {
}

} // namespace llpm

