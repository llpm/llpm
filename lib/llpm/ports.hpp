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

class OutputPort : public Port {
public:
    OutputPort(Block* owner, llvm::Type* type, std::string name = "");
    ~OutputPort();

    // Deleted defaults
    OutputPort(const OutputPort&) = delete;
    OutputPort* operator=(const OutputPort&) = delete;
};

} //llpm

#endif // __LLPM_PORTS_HPP__
