#ifndef __LLPM_INTRINSICS_HPP__
#define __LLPM_INTRINSICS_HPP__

#include <llpm/block.hpp>

namespace llpm {

class CommunicationIntrinsic: public Block {
protected:
    virtual bool hasState() const {
        return false;
    }
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

// Accepts a single input and stores it. Outputs the stored
// input when a request arrives. Also supports a reset signal,
// which invalidates the contents. Requests on invalid data are held
// until input data arrives.
class Register : public Block {
    InputPort _din;
    InputPort _reset;
    InputPort _req;
    OutputPort _dout;

public:
    Register(llvm::Type* type);
    virtual ~Register() { }

    virtual bool hasState() const {
        return true;
    }

    DEF_GET(din);
    DEF_GET(reset);
    DEF_GET(req);
    DEF_GET(dout);
};

} // namespace llpm

#endif // __LLPM_INTRINSICS_HPP__
