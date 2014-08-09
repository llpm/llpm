#ifndef __LLPM_COMM_INTR_HPP__
#define __LLPM_COMM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>

namespace llpm {

class CommunicationIntrinsic: public Block {
protected:
    virtual bool hasState() const {
        return false;
    }
};

// Convert one data type to another. Usually compiles down to a
// no-op
class Cast : public CommunicationIntrinsic {
    InputPort _din;
    OutputPort _dout;
    llvm::CastInst* _cast;
public:
    Cast(llvm::CastInst* cast);
    virtual ~Cast() { }

    DEF_GET(din);
    DEF_GET(cast);
    DEF_GET(dout);
};

// Takes N inputs and concatenates them into one output
class Join : public CommunicationIntrinsic {
    vector<InputPort> _din;
    OutputPort _dout;
public:
    Join(const vector<llvm::Type*>& inputs);
    virtual ~Join() { }

    DEF_ARRAY_GET(din);
    DEF_GET(dout);
};

// Takes N inputs of the same type and outputs them in 
// the order in which they arrive
class Select : public CommunicationIntrinsic {
    vector<InputPort> _din;
    OutputPort _dout;
public:
    Select(unsigned N, llvm::Type* type);
    virtual ~Select() { }

    DEF_ARRAY_GET(din);
    DEF_GET(dout);
};

// Takes N inputs of different types and outputs them
// in the order in which they arrive
class SelectUnion : public CommunicationIntrinsic {
    vector<InputPort> _din;
    vector<OutputPort> _dout;
public:
    SelectUnion(const vector<llvm::Type*>& inputs);
    // Note: unimplemented for now since LLVM lacks a union type!
    virtual ~SelectUnion() { }

    DEF_ARRAY_GET(din);
    DEF_ARRAY_GET(dout);
};


// Splits a single input into N constituent parts
class Split : public CommunicationIntrinsic {
    InputPort _din;
    vector<OutputPort> _dout;

public:
    Split(const vector<llvm::Type*>& outputs);
    virtual ~Split() { }

    DEF_GET(din);
    DEF_ARRAY_GET(dout);
};

// Selects one of N inputs and outputs it. Non-selected messages are
// destroyed
class Multiplexer : public CommunicationIntrinsic {
    vector<InputPort> _din;
    InputPort _sel;
    OutputPort _dout;

public:
    Multiplexer(unsigned N, llvm::Type* type);
    virtual ~Multiplexer() { }

    DEF_ARRAY_GET(din);
    DEF_GET(sel);
    DEF_GET(dout);
};

// Sends a single input to one of N outputs
class Router : public CommunicationIntrinsic {
    InputPort _din;
    InputPort _sel;
    vector<OutputPort> _dout;

public:
    Router(unsigned N, llvm::Type* type);
    virtual ~Router() { }

    DEF_GET(din);
    DEF_GET(sel);
    DEF_ARRAY_GET(dout);
};

} // namespace llpm

#endif // __LLPM_COMintrinsics.hppM_INTR_HPP__
