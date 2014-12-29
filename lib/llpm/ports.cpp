#include "ports.hpp"

#include <llpm/block.hpp>

namespace llpm {

InputPort::InputPort(Block* owner, llvm::Type* type, std::string name) :
    Port(owner, type, name)
{
    owner->definePort(this);
}

InputPort::~InputPort() { }

OutputPort::OutputPort(
    Block* owner, llvm::Type* type,
    std::string name) :
    Port(owner, type, name)
{
    owner->definePort(this);
}

OutputPort::~OutputPort() { }

DependenceRule OutputPort::depRule() const {
    return owner()->depRule(this);
}

const std::vector<InputPort*>& OutputPort::deps() const {
    return owner()->deps(this);
}

bool OutputPort::pipelineable() const {
    return owner()->pipelineable(this);
}

} // namespace llpm

