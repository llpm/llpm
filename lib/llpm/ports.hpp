#ifndef __LLPM_PORTS_HPP__
#define __LLPM_PORTS_HPP__

#include <util/macros.hpp>
#include <llpm/exceptions.hpp>

#include <boost/intrusive_ptr.hpp>
#include <llvm/IR/Type.h>

namespace llpm {

class Block;
typedef boost::intrusive_ptr<Block> BlockP;

class OutputPort;
class InputPort;
class Join;
class Split;
class ConnectionDB;

/**
 * Ports define the I/O channels of blocks. Each block contains one or
 * more output port and potentially some input channels. Input ports
 * can be connected to output ports and vise versa.
 */
class Port {
protected:
    Block* _owner;
    llvm::Type* _type;
    std::string _name;

    Port(Block* owner,
         llvm::Type* type,
         std::string name);
public:
    virtual ~Port();

    llvm::Type* type() const {
        return _type;
    }

    BlockP ownerP() const;

    Block* owner() const {
        return _owner;
    }

    void reset(llvm::Type* ty) {
        _type = ty;
    }

    DEF_GET_NP(name);
    DEF_SET(name);

    InputPort* asInput();
    const InputPort* asInput() const;

    OutputPort* asOutput();
    const OutputPort* asOutput() const;

    unsigned num() const;
};

/**
 * InputPorts accept tokens from OutputPorts
 */
class InputPort : public Port {
    /**
     * Functions often need to join the input. Make codegen prog. code
     * mildly easier by creating it on request.
     */
    Join* _join;

public:
    InputPort(Block* owner, llvm::Type* type, std::string name = "");
    ~InputPort();

    // Deleted defaults
    InputPort(const InputPort&) = delete;
    InputPort* operator=(const InputPort&) = delete;

    /**
     * Get a join block whose output is connected to this input.
     * This is a convenience function since lots of stuff ends up
     * driving a join which drives a block.
     */
    Join* join(ConnectionDB& conns);
    InputPort* join(ConnectionDB& conns, unsigned idx);
};

/**
 * Describes internal relationship of outputs to inputs.
 */
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
    DependenceRule() :
        _inputType(Custom),
        _outputType(Maybe)
    { }

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

    DependenceRule operator+(const DependenceRule& dr) const {
        DependenceRule ret;
        if (dr._inputType == _inputType)
            ret._inputType = _inputType;
        else
            ret._inputType = Custom;

        if (dr._outputType == _outputType)
            ret._outputType = _outputType;
        else
            ret._outputType = Maybe;
        return ret;
    }
};

/**
 * Outputs tokens. Can output them to multiple input ports.
 */
class OutputPort : public Port {
    /**
     * Functions often need to split the input. Make codegen prog.
     * code mildly easier by creating it on request.
     */
    Split* _split;

public:
    OutputPort(Block* owner, llvm::Type* type,
               std::string name = "");
    ~OutputPort();

    // Deleted defaults
    OutputPort(const OutputPort&) = delete;
    OutputPort* operator=(const OutputPort&) = delete;

    DependenceRule depRule() const;
    const std::vector<InputPort*>& deps() const;

    /**
     * Things receiving data often need only one component of said
     * data. This convenience function splits the data coming from
     * this output port.
     */
    Split* split(ConnectionDB& conns);
    OutputPort* split(ConnectionDB& conns, unsigned idx);
};


/* Out of line method definitions
 *   I want these inlined, but they can't be in the classes above because
 *   C++ is too stuck in the 70's.
 */

inline InputPort* Port::asInput() {
    return dynamic_cast<InputPort*>(this);
}

inline const InputPort* Port::asInput() const {
    return dynamic_cast<const InputPort*>(this);
}

inline OutputPort* Port::asOutput() {
    return dynamic_cast<OutputPort*>(this);
}

inline const OutputPort* Port::asOutput() const {
    return dynamic_cast<const OutputPort*>(this);
}

} //llpm

#endif // __LLPM_PORTS_HPP__
