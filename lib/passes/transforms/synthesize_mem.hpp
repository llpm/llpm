#pragma once

#include <passes/pass.hpp>
namespace llpm {

// Fwd defs. Have a run out of C++ insults? Surely not!
class Register;
class FiniteArray;
class ConnectionDB;

class SynthesizeMemoryPass : public ModulePass {
protected:
    virtual void runInternal(Module* mod);
    void synthesizeReg(ConnectionDB*, Register*);
    void synthesizeFiniteArray(ConnectionDB*, FiniteArray*);

public:
    SynthesizeMemoryPass(Design& d) :
        ModulePass(d) { }
};

} // namespace llpm

