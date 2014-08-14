#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/connection.hpp>

namespace llpm {

class Module : public Block {
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

    virtual const vector<Block&>& blocks() const = 0;
};

/**********
 * This module simply contains blocks. It is likely to be the most
 * common module and a good default.
 */
class ContainerModule : public Module {
    // The blocks which I directly contain
    set<Block*> _blocks;

    // The connections between blocks in this module
    ConnectionDB _conns;

public:
    ContainerModule(std::string name) :
        Module(name),
        _conns(this)
    { }
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
