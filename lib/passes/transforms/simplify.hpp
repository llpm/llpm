#ifndef __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__
#define __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__

#include <llpm/ports.hpp>
#include <passes/pass.hpp>

namespace llpm {

// Fwd defs. I'll stop insulting C++ now. It feels like beating a dead
// horse. Too bad it's not actually dead.
class Transformer;

class SimplifyPass : public ModulePass {
public:
    SimplifyPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

class CanonicalizeInputs: public ModulePass {
    void canonicalizeInput(Transformer& t, InputPort*);
public:
    CanonicalizeInputs(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};


};

#endif // __LLPM_PASSES_TRANSFORMS_SIMPLIFY_HPP__
