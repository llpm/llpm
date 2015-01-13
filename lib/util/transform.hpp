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

    bool canMutate() const {
        return _conns != NULL;
    }

    ConnectionDB* conns() {
        return _conns;
    }

    // Remove a block and rewire its connections
    void remove(Block* b);

    // Remove a block and ignore its connections
    void trash(Block* b);

    // Replace a block with another. Block can have only 1 input to avoid
    // mapping issues.
    void replace(Block* b, Block* with);

    // Break a connection and insert someone in its place
    void insertBetween(Connection, Block* b);
    void insertBetween(Connection, InputPort*, OutputPort*);

    void insertAfter(OutputPort*, Block* b);
    void insertAfter(OutputPort*, InputPort*, OutputPort*);
};

};

#endif // __LLPM_UTIL_TRANSFORM_HPP__

