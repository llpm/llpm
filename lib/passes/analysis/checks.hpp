#ifndef __LLPM_PASSES_ANALYSIS_CHECKS_HPP__
#define __LLPM_PASSES_ANALYSIS_CHECKS_HPP__

#include <passes/pass.hpp>

namespace llpm {

class CheckConnectionsPass: public ModulePass {
public:
    CheckConnectionsPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

class CheckOutputsPass: public ModulePass {
public:
    CheckOutputsPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

};

#endif // __LLPM_PASSES_ANALYSIS_CHECKS_HPP__
