#ifndef __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__
#define __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__

#include <passes/pass.hpp>

namespace llpm {

class SynthesizeMemoryPass : public ModulePass {
protected:
    virtual void runInternal(Module* mod);
public:
    SynthesizeMemoryPass(Design& d) :
        ModulePass(d) { }
};

} // namespace llpm

#endif // __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__
