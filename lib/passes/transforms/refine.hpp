#pragma once

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

