#ifndef __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__
#define __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>

/*********
 * Communication intrinsics govern the flow of information in an
 * LLPM design. All backends must directly support these intrinsics.
 */

namespace llpm {

class CommunicationIntrinsic: public virtual Block {
protected:
    virtual bool hasState() const {
        return false;
    }
};

// Identity function. Does nothing
class Identity: public CommunicationIntrinsic, public Function {
public:
    Identity(llvm::Type*);
    virtual ~Identity() { }

    virtual bool hasState() const {
        return false;
    }
};

// Waits for inputs to arrive on N control channels and one data
// channel then passes through the data token.
class Wait: public CommunicationIntrinsic {
    InputPort _din;
    OutputPort _dout;
    std::vector<InputPort*> _controls;

public:
    Wait(llvm::Type*);
    virtual ~Wait();

    DEF_GET(din);
    DEF_GET(dout);
    DEF_ARRAY_GET(controls);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }

    InputPort* newControl(llvm::Type*);
};

// Convert one data type to another. Usually compiles down to a
// no-op
class Cast : public CommunicationIntrinsic, public Function {
    llvm::CastInst* _cast;
public:
    Cast(llvm::CastInst* cast);
    virtual ~Cast() { }

    DEF_GET(cast);

    virtual bool hasState() const {
        return false;
    }
};

// Takes N inputs and concatenates them into one output
class Join : public CommunicationIntrinsic {
    std::vector<InputPort*> _din;
    OutputPort _dout;
public:
    Join(const std::vector<llvm::Type*>& inputs);
    Join(llvm::Type* output);
    virtual ~Join() { }

    DEF_ARRAY_GET(din);
    DEF_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

// Takes N inputs of the same type and outputs them in 
// the order in which they arrive
class Select : public CommunicationIntrinsic {
    std::vector<InputPort*> _din;
    OutputPort _dout;
public:
    Select(unsigned N, llvm::Type* type);
    virtual ~Select() { }

    DEF_ARRAY_GET(din);
    DEF_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::OR,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

// Takes N inputs of different types and outputs them
// in the order in which they arrive
class SelectUnion : public CommunicationIntrinsic {
    std::vector<InputPort*> _din;
    OutputPort _dout;
public:
    SelectUnion(const std::vector<llvm::Type*>& inputs);
    // Note: unimplemented for now since LLVM lacks a union type!
    virtual ~SelectUnion() { }

    DEF_ARRAY_GET(din);
    DEF_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::OR,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

// Splits a single input into N constituent parts
class Split : public CommunicationIntrinsic {
    InputPort _din;
    std::vector<OutputPort*> _dout;

public:
    Split(const std::vector<llvm::Type*>& outputs);
    Split(llvm::Type* input);
    virtual ~Split() { }

    DEF_GET(din);
    DEF_ARRAY_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(std::find(_dout.begin(), _dout.end(), op) != _dout.end());
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(std::find(_dout.begin(), _dout.end(), op) != _dout.end());
        return inputs();
    }
};

// Extract a single element from a message
class Extract : public CommunicationIntrinsic, public Function {
    std::vector<unsigned> _path;

public:
    Extract(llvm::Type* t, std::vector<unsigned> path);
    virtual ~Extract() { }

    virtual bool hasState() const {
        return false;
    }

    DEF_GET_NP(path);
};

// Selects one of N inputs and outputs it. Non-selected messages are
// destroyed
// Input data type:
// { sel : Log2 (#  inputs), Input 1, Input 2, ... Input N }
class Multiplexer : public CommunicationIntrinsic, public Function {
    static llvm::Type* GetInput(unsigned N, llvm::Type* type);

public:
    Multiplexer(unsigned N, llvm::Type* type);
    virtual ~Multiplexer() { }

    virtual bool hasState() const {
        return false;
    }
};

// Sends a single input to one of N outputs
// Input data type:
// { sel: Log2 (# outputs), Input Data }
class Router : public CommunicationIntrinsic {
    static llvm::Type* GetInput(unsigned N, llvm::Type* type);
    InputPort _din;
    std::vector<OutputPort*> _dout;

public:
    Router(unsigned N, llvm::Type* type);
    virtual ~Router() { }

    DEF_GET(din);
    DEF_ARRAY_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(std::find(_dout.begin(), _dout.end(), op) != _dout.end());
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Maybe);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(std::find(_dout.begin(), _dout.end(), op) != _dout.end());
        return inputs();
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__
