#include "ports.hpp"

#include <llpm/block.hpp>

namespace llpm {

InputPort::InputPort(Block* owner, llvm::Type* type) :
    Port(owner, type)
{
    owner->definePort(this);
}

InputPort::~InputPort() { }

OutputPort::OutputPort(Block* owner, llvm::Type* type) :
    Port(owner, type)
{
    owner->definePort(this);
}

OutputPort::~OutputPort() { }

} // namespace llpm

