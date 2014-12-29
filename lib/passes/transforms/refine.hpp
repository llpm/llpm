#ifndef __LLPM_PASSES_TRANSFORMS_REFINE_HPP__
#define __LLPM_PASSES_TRANSFORMS_REFINE_HPP__

#include <passes/pass.hpp>

namespace llpm {

class RefinePass : public ModulePass {
public:
    RefinePass(Design& d) :
        ModulePass(d)
    { }

    virtual void runInternal(Module*);
};

} // namespace llpm

#endif // __LLPM_PASSES_TRANSFORMS_REFINE_HPP__
