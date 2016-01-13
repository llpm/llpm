#pragma once

#include <llpm/ports.hpp>
#include <passes/pass.hpp>

#include <set>

namespace llpm {

// Fwd defs. I'll stop insulting C++ now. It feels like beating a dead
// horse. Too bad it's not actually dead.
class Transformer;
class Wait;

class SimplifyPass : public ModulePass {

    void eliminateNoops(Module* mod);
    void simplifyNullSinks(Module* mod);
    void simplifyExtracts(Module* mod);
    void simplifyConstants(Module* mod);

public:
    SimplifyPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

class SimplifyWaits:  public ModulePass {
    void collectControls(
        Transformer& t, Wait*);
public:
    SimplifyWaits(Design& d) :
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

