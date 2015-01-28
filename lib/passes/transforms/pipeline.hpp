#ifndef __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__
#define __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__

#include <passes/pass.hpp>
#include <util/time.hpp>

namespace llpm {

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

class PipelineFrequencyPass: public ModulePass {
    Time _maxDelay;
public:
    PipelineFrequencyPass(Design& d, Time maxDelay) :
        ModulePass(d),
        _maxDelay(maxDelay)
    { }

    virtual void runInternal(Module*);
};

class LatchUntiedOutputs: public ModulePass {
public:
    LatchUntiedOutputs(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};


} // namespace llpm

#endif // __LLPM_PASSES_TRANSFORMS_PIPELINE_HPP__
