#pragma once

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

class CheckCyclesPass: public ModulePass {
public:
    CheckCyclesPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

};

