#ifndef __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__
#define __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__

#include <passes/pass.hpp>
#include <llpm/time.hpp>

#include <map>

namespace llpm {

// fwd defs
class OutputPort;

class PipelineDependentsPass: public ModulePass {
public:
    PipelineDependentsPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

class PipelineCyclesPass: public ModulePass {
public:
    PipelineCyclesPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

class PipelineFrequencyPass: public Pass {
    Time _maxDelay;
    std::map<const OutputPort*, Time> _modOutDelays;

    friend struct DelayVisitor;
    bool runOnModule(Module* mod, Time initTime);

public:
    PipelineFrequencyPass(Design& d, Time maxDelay) :
        Pass(d),
        _maxDelay(maxDelay)
    { }

    virtual bool run();
};

class LatchUntiedOutputs: public ModulePass {
    bool _useRegs;

public:
    LatchUntiedOutputs(Design& d, bool useRegs) :
        ModulePass(d),
        _useRegs(useRegs)
    { }

    virtual void runInternal(Module*);
};


} // namespace llpm

#endif // __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__
