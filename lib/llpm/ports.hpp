#ifndef __LLPM_PORTS_HPP__
#define __LLPM_PORTS_HPP__

#include <util/macros.hpp>
#include <llpm/exceptions.hpp>

#include <boost/intrusive_ptr.hpp>
#include <llvm/IR/Type.h>

#include <initializer_list>

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
    virtual bool isInput() const = 0;

    OutputPort* asOutput();
    const OutputPort* asOutput() const;
    virtual bool isOutput() const = 0;

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

    bool isInput() const {
        return true;
    }

    bool isOutput() const {
        return false;
    }
};

/**
 * Describes internal relationship of outputs to inputs.
 */
struct DependenceRule {
    friend class OutputPort;
public:
    typedef llvm::SmallVector<const InputPort*, 2> InputVec;

    enum DepType {
        AND_FireOne,
        Custom
    };

public:
    DepType depType;
    InputVec inputs;

    DependenceRule() :
        depType(Custom)
    { }

    DependenceRule(DepType ty) :
        depType(ty)
    { }

    template<unsigned N>
    DependenceRule(DepType ty,
                   llvm::SmallVector<InputPort*, N> inputs) :
        depType(ty),
        inputs(inputs.begin(), inputs.end())
    { }

    DependenceRule(DepType ty,
                   std::vector<InputPort*> inputs) :
        depType(ty),
        inputs(inputs.begin(), inputs.end())
    { }

    DependenceRule(DepType ty,
                   std::initializer_list<const InputPort*> inputs) :
        depType(ty),
        inputs(inputs)
    { }

    template<unsigned N>
    DependenceRule(DepType ty,
                   llvm::SmallVector<const InputPort*, N> inputs) :
        depType(ty),
        inputs(inputs.begin(), inputs.end())
    { }

    DependenceRule(DepType ty,
                   std::vector<const InputPort*> inputs) :
        depType(ty),
        inputs(inputs.begin(), inputs.end())
    { }

    bool operator==(const DependenceRule& dr) const {
        return dr.depType == this->depType &&
               dr.inputs == this->inputs;
    }

    bool operator!=(const DependenceRule& dr) const {
        return !operator==(dr);
    }

    DependenceRule operator+(const DependenceRule& dr) const {
        if (*this == dr)
            return *this;

        DependenceRule ret(Custom, this->inputs);
        ret.inputs.append(dr.inputs.begin(), dr.inputs.end());
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

    DependenceRule deps() const;

    /**
     * Things receiving data often need only one component of said
     * data. This convenience function splits the data coming from
     * this output port.
     */
    Split* split(ConnectionDB& conns);
    OutputPort* split(ConnectionDB& conns, unsigned idx);

    bool isInput() const {
        return false;
    }

    bool isOutput() const {
        return true;
    }
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
