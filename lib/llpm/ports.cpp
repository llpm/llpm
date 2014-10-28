#include "ports.hpp"

#include <llpm/block.hpp>

namespace llpm {

InputPort::InputPort(Block* owner, llvm::Type* type, std::string name) :
    Port(owner, type, name)
{
    owner->definePort(this);
}

InputPort::~InputPort() { }

OutputPort::OutputPort(Block* owner, llvm::Type* type, std::string name) :
    Port(owner, type, name)
{
    owner->definePort(this);
}

OutputPort::~OutputPort() { }

} // namespace llpm

