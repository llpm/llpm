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

    // Remove a block and rewire its connections
    void remove(Block* b);

    // Remove a block and ignore its connections
    void trash(Block* b);

    // Replace a block with another. Block can have only 1 input to avoid
    // mapping issues.
    void replace(Block* b, Block* with);
};

};

#endif // __LLPM_UTIL_TRANSFORM_HPP__

