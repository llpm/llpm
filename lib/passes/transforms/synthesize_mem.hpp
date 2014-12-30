#ifndef __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__
#define __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__

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

#endif // __LLPM_PASSES_TRANSFORMS_SYNTHESIZE_MEM_HPP__
