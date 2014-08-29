#ifndef __LLPM_LOGIC_INTR_HPP__
#define __LLPM_LOGIC_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constant.h>

namespace llpm {

// Computes F boolean functions from an N-bit input
//   Automatically casts input and output types to and from the
//   bit arrays upon which this function operates.
class BooleanLogic: public Function {
public:
    BooleanLogic(unsigned F,
                 llvm::Type* inputType,
                 llvm::Type* outputType);
    virtual ~BooleanLogic() { }
};

// Constant value. Should probably be treated specially by backends
// and optimizers.
class Constant : public Block {
    llvm::Constant* _value;
    OutputPort   _dout;
public:
    Constant(llvm::Value* value) :
        _dout(this, value->getType())
    {
        _value = llvm::dyn_cast<llvm::Constant>(value);
        if (_value == NULL) {
            throw InvalidArgument("Value passed to constant must be Constant!");
        }
    }

    Constant(llvm::Constant* value) :
        _value(value),
        _dout(this, value->getType())
    { }

    virtual bool hasState() const { return false; }

    DEF_GET(dout);
    DEF_GET(value);
    DEF_SET_NONULL(value);
};

} // namespace llpm

#endif // __LLPM_MEM_INTR_HPP__
