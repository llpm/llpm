#pragma once

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

    /**
     * Given an output port which is indirectly constant, return an equivalent
     * single constant.
     */
    static Constant* getEquivalent(OutputPort*);

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND_FireOne);
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

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::Custom);
    }
};

// A block which absorbs and destroys tokens
class NullSink: public virtual Block {
    InputPort   _din;
public:
    NullSink(llvm::Type* t) :
        _din(this, t, "in")
    { }

    virtual bool hasState() const { return false; }

    DEF_GET(din);

    virtual DependenceRule deps(const OutputPort*) const {
        throw InvalidArgument("Output port does not belong to this block!");
    }
};

// Produces one token, once immediate after reset
class Once: public virtual Block {
    llvm::Constant* _value;
    OutputPort _dout;
public:
    Once(llvm::Value* value) :
        _dout(this, value->getType(), "c")
    {
        _value = llvm::dyn_cast<llvm::Constant>(value);
        if (_value == NULL) {
            throw InvalidArgument("Value passed to constant must be Constant!");
        }
    }

    Once(llvm::Constant* value) :
        _value(value),
        _dout(this, value->getType(), "c")
    { }

    Once(llvm::Type* t) :
        _value(NULL),
        _dout(this, t, "c")
    { }

    virtual bool hasState() const { return true; }
    virtual std::string print() const;

    DEF_GET(dout);
    DEF_GET_NP(value);
    DEF_SET_NONULL(value);

    static Once* getVoid(Design& d) {
        return new Once(llvm::Type::getVoidTy(d.context()));
    }

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::Custom);
    }
};

} // namespace llpm

