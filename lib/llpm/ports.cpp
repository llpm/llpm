#include "ports.hpp"

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/module.hpp>
#include <libraries/core/comm_intr.hpp>

namespace llpm {

Port::Port(Block* owner,
           llvm::Type* type,
           std::string name) :
    _owner(owner),
    _type(type),
    _name(name)
{ }

Port::~Port() { }

BlockP Port::ownerP() const {
    return _owner->getptr();
}

unsigned Port::num() const {
    if (asInput() != NULL)
        return owner()->inputNum(asInput());
    else
        return owner()->outputNum(asOutput());
}

InputPort::InputPort(Block* owner, llvm::Type* type, std::string name) :
    Port(owner, type, name),
    _join(NULL)
{
    owner->definePort(this);
}

InputPort::~InputPort() { }

Join* InputPort::join(ConnectionDB& conns) {
    if (_join == NULL) {
        _join = new Join(_type);
        conns.connect(_join->dout(), this);
    }
    return _join;
}

InputPort* InputPort::join(ConnectionDB& conns, unsigned idx) {
    return join(conns)->din(idx);
}

OutputPort::OutputPort(
    Block* owner, llvm::Type* type,
    std::string name) :
    Port(owner, type, name),
    _split(NULL)
{
    owner->definePort(this);
}

OutputPort::~OutputPort() { }

DependenceRule OutputPort::deps() const {
    return owner()->deps(this);
}

Split* OutputPort::split(ConnectionDB& conns) {
    if (_split == NULL) {
        _split = new Split(_type);
        conns.connect(_split->din(), this);
    }
    return _split;
}

OutputPort* OutputPort::split(ConnectionDB& conns, unsigned idx) {
    return split(conns)->dout(idx);
}

} // namespace llpm

