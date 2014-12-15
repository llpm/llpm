#ifndef __LLPM_CONTROL_REGION_HPP__
#define __LLPM_CONTROL_REGION_HPP__

#include <llpm/module.hpp>
#include <passes/pass.hpp>

namespace llpm {

class ControlRegion : public ContainerModule {
    MutableModule* _parent;
    bool add(Block*);

public:
    ControlRegion(MutableModule* parent, Block* seed, std::string name="") :
        ContainerModule(parent->design(), name),
        _parent(parent) {
        this->module(parent);
        auto rc = add(seed);
        assert(rc);
    }

    bool add(Connection);

    bool growUp();
    bool growDown();
    bool grow();

    bool canGrow(InputPort* ip);
    bool canGrow(OutputPort* op);
    bool canGrow(Port* p);
};

class FormControlRegionPass : public ModulePass {
public:
    FormControlRegionPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void run(Module*);
};

} // namespace llpm

#endif // __LLPM_CONTROL_REGION_HPP__

