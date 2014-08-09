#ifndef __LLPM_LOGIC_INTR_HPP__
#define __LLPM_LOGIC_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>

namespace llpm {

// Computes F boolean functions from an N-bit input
class BooleanLogic: public Function {

public:
    BooleanLogic(unsigned F,
                 llvm::Type* inputType,
                 llvm::Type* outputType);
    virtual ~BooleanLogic() { }
};

} // namespace llpm

#endif // __LLPM_MEM_INTR_HPP__
