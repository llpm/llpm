#include "objects.hpp"

#include <llvm/IR/Function.h>

namespace llpm {

LLVMFunction::LLVMFunction(llpm::Design& design, llvm::Function* func) :
    ContainerModule(design, func->getName())
{
    build(func);
}

LLVMFunction::~LLVMFunction() {
}

void LLVMFunction::build(llvm::Function* func) {
}

} // namespace llpm

