#ifndef __LLPM_PORTS_HPP__
#define __LLPM_PORTS_HPP__

#include <llvm/IR/Type.h>
#include <util/macros.hpp>

namespace llpm {

class Block;
class OutputPort;

class Port {
protected:
    Block* _owner;
    llvm::Type* _type;
    std::string _name;

    Port(Block* owner,
         llvm::Type* type,
         std::string name) :
        _owner(owner),
        _type(type),
        _name(name)
    { }

    virtual ~Port() { }

public:
    llvm::Type* type() const {
        return _type;
    }

    Block* owner() const {
        return _owner;
    }

    void reset(llvm::Type* ty) {
        _type = ty;
    }

    DEF_GET_NP(name);
    DEF_SET(name);
};

class InputPort : public Port {
public:
    InputPort(Block* owner, llvm::Type* type, std::string name = "");
    ~InputPort();

    // Deleted defaults
    InputPort(const InputPort&) = delete;
    InputPort* operator=(const InputPort&) = delete;
};

// Describes internal relationship of outputs to inputs.
struct DependenceRule {
    friend class OutputPort;
public:
    enum InputType {
        AND,
        OR,
        Custom
    };

    enum OutputType {
        Always,
        Maybe
    };

protected:
    InputType       _inputType;
    OutputType      _outputType;

public:
    DependenceRule(InputType inpTy,
                   OutputType outTy) :
        _inputType(inpTy),
        _outputType(outTy) { }

    DEF_GET_NP(inputType);
    DEF_GET_NP(outputType);
    DEF_SET(inputType);
    DEF_SET(outputType);

    bool operator==(const DependenceRule& dr) const {
        return dr._inputType == this->_inputType &&
               dr._outputType == this->_outputType;
    }

    bool operator!=(const DependenceRule& dr) const {
        return !operator==(dr);
    }
};

class OutputPort : public Port {
public:
    OutputPort(Block* owner, llvm::Type* type,
               std::string name = "");
    ~OutputPort();

    // Deleted defaults
    OutputPort(const OutputPort&) = delete;
    OutputPort* operator=(const OutputPort&) = delete;

    DependenceRule depRule() const;
    const std::vector<InputPort*>& deps() const;

    bool pipelineable() const;
};

} //llpm

#endif // __LLPM_PORTS_HPP__
