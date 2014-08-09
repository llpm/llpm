#ifndef __LLPM_PORTS_HPP__
#define __LLPM_PORTS_HPP__

#include <llvm/IR/Type.h>

namespace llpm {

class Block;

class Port {
protected:
    Block* owner;
    llvm::Type* _type;

    Port(Block* owner, llvm::Type* type) :
        _type(type)
    { }

    virtual ~Port() { }

public:
    const llvm::Type* type() const {
        return _type;
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
