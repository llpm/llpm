#include "memory.hpp"

#include <llvm/IR/Type.h>

namespace llpm {

RTLReg::RTLReg(llvm::Type* dt) :
    _type(dt),
    _write(this, dt, llvm::Type::getVoidTy(dt->getContext()), true, "write"),
    _read(this, dt, "read")
{ }

} // namespace llpm
