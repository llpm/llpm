#ifndef __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__
#define __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__

#include <passes/pass.hpp>

namespace llpm {

class SimplifyPass : public ModulePass {
public:
    SimplifyPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void run(Module*);
};

};

#endif // __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__
