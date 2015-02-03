#ifndef __LLPM_CONTROL_REGION_HPP__
#define __LLPM_CONTROL_REGION_HPP__

#include <llpm/module.hpp>
#include <passes/pass.hpp>

namespace llpm {

// Fwd. defs
class PipelineRegister;

/**
 * A ControlRegion is a group of blocks with the following properties:
 *   - All of them have AND input firing rules
 *   - All of them are "tied" outputs
 *   - All of the outputs rely on all of the inputs
 *
 * This definition means that it is trivial to statically schedule the
 * blocks contained by a CR. At a minimum, the control bits (valid and
 * backpressure) can be amortized over all the blocks.
 *
 * Finally, the CR also unifies its output to create just one output.
 * I did this for a reason, but can't remember what that reason was.
 */
class ControlRegion : public ContainerModule {
    MutableModule* _parent;
    bool _finalized;
    bool add(Block*, const std::set<Port*>& constPorts = {});

    bool _scheduled;
    std::vector<std::set<Block*>> _blockSchedule;
    std::vector<std::set<PipelineRegister*>> _regSchedule;

    void schedule();

    std::set<InputPort*> findDependences(OutputPort*) const;

public:
    ControlRegion(MutableModule* parent,
                  Block* seed,
                  std::string name="") :
        ContainerModule(parent->design(), name),
        _parent(parent),
        _finalized(false),
        _scheduled(false) {
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

    /// How many clock cycles does it take to compute this CR?
    unsigned clocks();
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

