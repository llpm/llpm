#ifndef __LLPM_LIBRARIES_CORE_LOGIC_INTR_HPP__
#define __LLPM_LIBRARIES_CORE_LOGIC_INTR_HPP__

#include <llpm/design.hpp>
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
class Constant : public virtual Block {
    llvm::Constant* _value;
    OutputPort   _dout;
public:
    Constant(llvm::Value* value) :
        _dout(this, value->getType(),
              "c")
    {
        _value = llvm::dyn_cast<llvm::Constant>(value);
        if (_value == NULL) {
            throw InvalidArgument("Value passed to constant must be Constant!");
        }
    }

    Constant(llvm::Constant* value) :
        _value(value),
        _dout(this, value->getType(), "c")
    { }

    Constant(llvm::Type* t) :
        _value(NULL),
        _dout(this, t, "c")
    { }

    virtual bool hasState() const { return false; }
    virtual std::string print() const;

    DEF_GET(dout);
    DEF_GET_NP(value);
    DEF_SET_NONULL(value);

    static Constant* getVoid(Design& d) {
        return new Constant(llvm::Type::getVoidTy(d.context()));
    }

    virtual const std::vector<InputPort*>& deps(const OutputPort*) const {
        return inputs();
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }
};

// A data source which never produces tokens. Basically the opposite
// of a constant.
class Never: public virtual Block {
    OutputPort   _dout;
public:
    Never(llvm::Type* t) :
        _dout(this, t, "c")
    { }

    virtual bool hasState() const { return false; }

    DEF_GET(dout);

    virtual const std::vector<InputPort*>& deps(const OutputPort*) const {
        return inputs();
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Maybe);
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__
