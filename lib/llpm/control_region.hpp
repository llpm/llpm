#ifndef __LLPM_CONTROL_REGION_HPP__
#define __LLPM_CONTROL_REGION_HPP__

#include <llpm/module.hpp>
#include <passes/pass.hpp>

namespace llpm {

class ControlRegion : public ContainerModule {
    MutableModule* _parent;
    bool _finalized;
    bool add(Block*, const std::set<Port*>& constPorts = {});

    std::set<InputPort*> findDependences(OutputPort*) const;

public:
    ControlRegion(MutableModule* parent,
                  Block* seed,
                  std::string name="") :
        ContainerModule(parent->design(), name),
        _parent(parent),
        _finalized(false) {
        this->module(parent);
        auto rc = add(seed);
        assert(rc);
    }

    static bool BlockAllowed(Block*);

    bool add(Connection, const std::set<Port*>& constPorts = {});

    bool growUp(const std::set<Port*>& constPorts = {});
    bool growDown(const std::set<Port*>& constPorts = {});
    bool grow(const std::set<Port*>& constPorts = {});

    bool canGrow(InputPort* ip);
    bool canGrow(OutputPort* op);
    bool canGrow(Port* p);

    // Ensure that we have only one dependent output
    void finalize();
    DEF_GET_NP(finalized);

    virtual void validityCheck() const;

    virtual bool refine(ConnectionDB& conns) const;
    virtual bool refinable() const {
        return true;
    }

    virtual const std::vector<InputPort*>& deps(const OutputPort*) const;
    virtual DependenceRule depRule(const OutputPort* op) const;
};

class FormControlRegionPass : public ModulePass {
public:
    FormControlRegionPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

} // namespace llpm

#endif // __LLPM_CONTROL_REGION_HPP__

