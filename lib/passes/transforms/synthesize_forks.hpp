#ifndef __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_FORKS_HPP__
#define __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_FORKS_HPP__

#include <passes/pass.hpp>
namespace llpm {

// Fwd defs. Have a run out of C++ insults? Surely not!
class ConnectionDB;

class SynthesizeForksPass : public ModulePass {
protected:
    virtual void runInternal(Module* mod);

public:
    SynthesizeForksPass(Design& d) :
        ModulePass(d) { }
};

} // namespace llpm

#endif // __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_FORKS_HPP__
