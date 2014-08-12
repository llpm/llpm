#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/ports.hpp>
#include <llpm/block.hpp>

namespace llpm {

class Module {
    std::string _name;
public:
    Module(std::string name) :
        _name(name)
    { }
    virtual ~Module();

    std::string name() { return _name; }

    virtual bool hasState() const = 0;
    virtual bool isPure() const {
        return !hasState();
    }

    virtual vector<InputPort&> inputPorts() const = 0;
    virtual vector<OutputPort&> outputPorts() const = 0;
    virtual vector<Block&> blocks();
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
