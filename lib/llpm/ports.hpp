#ifndef __LLPM_PORTS_HPP__
#define __LLPM_PORTS_HPP__

#include <llvm/IR/Type.h>
#include <util/macros.hpp>

namespace llpm {

class Block;

class Port {
protected:
    Block* _owner;
    llvm::Type* _type;

    Port(Block* owner, llvm::Type* type) :
        _owner(owner),
        _type(type)
    { }

    virtual ~Port() { }

public:
    llvm::Type* type() const {
        return _type;
    }

    Block* owner() const {
        return _owner;
    }
};

class InputPort : public Port {
public:
    InputPort(Block* owner, llvm::Type* type) :
        Port(owner, type)
    { }
};

class OutputPort : public Port {
public:
    OutputPort(Block* owner, llvm::Type* type) :
        Port(owner, type)
    { }
};

} //llpm

#endif // __LLPM_PORTS_HPP__
