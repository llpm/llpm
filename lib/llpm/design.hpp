#ifndef __LLPM_DESIGN_HPP__
#define __LLPM_DESIGN_HPP__

#include <llvm/IR/LLVMContext.h>

namespace llpm {

class Design {
public:
    llvm::LLVMContext context;
};

} // namespace llpm

#endif // __LLPM_DESIGN_HPP__
