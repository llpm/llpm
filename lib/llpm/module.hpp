#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/ports.hpp>
#include <llpm/block.hpp>

namespace llpm {

class Module {
    virtual bool hasState() const;
    virtual bool isPure() const {
        return !hasState();
    }
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
