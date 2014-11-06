#ifndef __LLPM_UTIL_TRANSFORM_HPP__
#define __LLPM_UTIL_TRANSFORM_HPP__

#include <llpm/module.hpp>
#include <llpm/connection.hpp>

namespace llpm {

class Transformer {
protected:
    Module* _mod;
    ConnectionDB* _conns;
public:
    Transformer(Module* mod) :
        _mod(mod),
        _conns(mod->conns())
    { }

    void remove(Block* b);
};

};

#endif // __LLPM_UTIL_TRANSFORM_HPP__

