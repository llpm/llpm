#ifndef __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__
#define __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>
#include <util/macros.hpp>
#include <util/llvm_type.hpp>
#include <memory>

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

/**
 * Identity function. Does nothing to the data but forward it on. Can
 * always be safely removed.
 */
class Identity: public CommunicationIntrinsic, public Function {
public:
    Identity(llvm::Type*);
    virtual ~Identity() { }

    virtual bool hasState() const {
        return false;
    }
};

/**
 * Waits for inputs to arrive on N control channels and one data
 * channel then passes through the data token.
 */
class Wait: public CommunicationIntrinsic {
    InputPort _din;
    OutputPort _dout;
    std::vector<std::unique_ptr<InputPort>> _controls;

public:
    Wait(llvm::Type*);
    virtual ~Wait() { }

    DEF_GET(din);
    DEF_GET(dout);
    DEF_UNIQ_ARRAY_GET(controls);

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
    void newControl(ConnectionDB*, OutputPort*);
};

/**
 * Convert one data type to another. The two inputs must be of the
 * same bit width. Usually compiles down to a no-op.
 */
class Cast : public CommunicationIntrinsic, public Function {
    llvm::CastInst* _cast;
public:
    Cast(llvm::CastInst* cast);
    Cast(llvm::Type* from, llvm::Type* to);
    virtual ~Cast() { }

    DEF_GET_NP(cast);

    virtual bool hasState() const {
        return false;
    }
};

/**
 * Takes N inputs and concatenates them into one output.
 */
class Join : public CommunicationIntrinsic {
    std::vector<std::unique_ptr<InputPort>> _din;
    OutputPort _dout;
public:
    Join(const std::vector<llvm::Type*>& inputs);
    Join(llvm::Type* output);
    virtual ~Join() { }

    DEF_UNIQ_ARRAY_GET(din);
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

    static Join* get(ConnectionDB&, const std::vector<OutputPort*>&);
};

/**
 * Takes N inputs of the same type and outputs them one at a time. The
 * order in which they are outputted is undefined.
 */
class Select : public CommunicationIntrinsic {
    std::vector<std::unique_ptr<InputPort>> _din;
    OutputPort _dout;
public:
    Select(unsigned N, llvm::Type* type);
    virtual ~Select() { }

    DEF_UNIQ_ARRAY_GET(din);
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

    float logicalEffort(InputPort*, OutputPort*) const {
        // Since this'll become a multiplexer, there's a little effort
        // involved
        return 1.0 + log2((float)_din.size());
    }

    InputPort* createInput();
};

/**
 * Takes N inputs of different types and outputs them in the order in
 * which they arrive.
 */
class SelectUnion : public CommunicationIntrinsic {
    std::vector<std::unique_ptr<InputPort>> _din;
    OutputPort _dout;
public:
    SelectUnion(const std::vector<llvm::Type*>& inputs);
    // Note: unimplemented for now since LLVM lacks a union type!
    virtual ~SelectUnion() { }

    DEF_UNIQ_ARRAY_GET(din);
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

    float logicalEffort(InputPort*, OutputPort*) const {
        // Since this'll become a multiplexer, there's a little effort
        // involved
        return 1.0 + log2((float)_din.size());
    }
};

/**
 * Splits a single input into N constituent parts.
 */
class Split : public CommunicationIntrinsic {
    InputPort _din;
    std::vector<std::unique_ptr<OutputPort>> _dout;

public:
    Split(const std::vector<llvm::Type*>& outputs);
    Split(llvm::Type* input);
    virtual ~Split() { }

    DEF_GET(din);
    DEF_UNIQ_ARRAY_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(std::find_if(_dout.begin(), _dout.end(),
                         RawEqualTo<const OutputPort>(op)) != _dout.end());
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(std::find_if(_dout.begin(), _dout.end(),
                         RawEqualTo<const OutputPort>(op)) != _dout.end());
        return inputs();
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const {
        return refineToExtracts(conns);
    }

    virtual bool refineToExtracts(ConnectionDB& conns) const;
};

/**
 * Extract a single element from a message.
 */
class Extract : public CommunicationIntrinsic, public Function {
    std::vector<unsigned> _path;
    llvm::Type* GetOutput(llvm::Type* t, std::vector<unsigned> path);

public:
    Extract(llvm::Type* t, std::vector<unsigned> path);
    virtual ~Extract() { }

    virtual bool hasState() const {
        return false;
    }

    DEF_GET_NP(path);

    virtual std::string print() const;
};

/**
 * Selects one of N inputs and outputs it. Non-selected messages are
 * destroyed.
 * Input data type:
 *   { sel : Log2 (#  inputs), Input 1, Input 2, ... Input N }
 */
class Multiplexer : public CommunicationIntrinsic, public Function {
    static llvm::Type* GetInput(unsigned N, llvm::Type* type);

public:
    Multiplexer(unsigned N, llvm::Type* type);
    virtual ~Multiplexer() { }

    virtual bool hasState() const {
        return false;
    }

    virtual float logicalEffortFunc() const {
        return 1.0 + log2((float)(numContainedTypes(din()->type()) - 1));
    }
};

/**
 * Sends a single input to one of N outputs.
 * Input data type:
 *   { sel: Log2 (# outputs), Input Data }
 */
class Router : public CommunicationIntrinsic {
    static llvm::Type* GetInput(unsigned N, llvm::Type* type);
    InputPort _din;
    std::vector<std::unique_ptr<OutputPort>> _dout;

public:
    Router(unsigned N, llvm::Type* type);
    virtual ~Router() { }

    DEF_GET(din);
    DEF_UNIQ_ARRAY_GET(dout);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(std::find_if(_dout.begin(), _dout.end(),
                         RawEqualTo<const OutputPort>(op)) != _dout.end());
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Maybe);
    }

    virtual bool outputsSeparate() const {
        return true;
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(std::find_if(_dout.begin(), _dout.end(),
                         RawEqualTo<const OutputPort>(op)) != _dout.end());
        return inputs();
    }

    float logicalEffort(InputPort*, OutputPort*) const {
        return 1.0 + log2((float)_dout.size());
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_COMM_INTR_HPP__
