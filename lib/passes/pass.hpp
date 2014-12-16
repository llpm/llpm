#ifndef __LLPM_PASSES_PASS_HPP__
#define __LLPM_PASSES_PASS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>

namespace llpm {

class Pass {
protected:
    Design& _design;

public:
    Pass(Design& design) :
        _design(design)
    { }

    virtual void run() = 0;
};

class ModulePass : public Pass {
protected:
    virtual void runInternal(Module* mod) = 0;
public:
    ModulePass(Design& design) :
        Pass(design)
    { }

    virtual void run();
    void run(Module* mod);
};

};

#endif // __LLPM_PASSES_PASS_HPP__

