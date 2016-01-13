#pragma once

#include <passes/pass.hpp>
namespace llpm {

// Fwd defs. Have a run out of C++ insults? Surely not!
class ConnectionDB;

class SynthesizeForksPass : public ModulePass {
    bool _pipeline;

protected:
    virtual void runInternal(Module* mod);

public:
    SynthesizeForksPass(Design& d, bool pipeline) :
        ModulePass(d),
        _pipeline(pipeline)
    { }
};

} // namespace llpm

