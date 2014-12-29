#ifndef __LLPM_PASSES_PASS_HPP__
#define __LLPM_PASSES_PASS_HPP__

namespace llpm {
    class Design;
    class Module;

class Pass {
protected:
    Design& _design;

public:
    Pass(Design& design) :
        _design(design)
    { }

    virtual bool run() = 0;
};

class ModulePass : public Pass {
protected:
    virtual void runInternal(Module* mod) = 0;
public:
    ModulePass(Design& design) :
        Pass(design)
    { }

    virtual bool run();
    bool run(Module* mod);
};

};

#endif // __LLPM_PASSES_PASS_HPP__

