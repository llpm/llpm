#ifndef __LLPM_SCHEDULED_REGION_HPP__
#define __LLPM_SCHEDULED_REGION_HPP__

#include <llpm/module.hpp>
#include <passes/pass.hpp>

namespace llpm {

// Fwd. defs
class PipelineRegister;
class PipelineStageController;
class Wait;

/**
 * A ScheduledRegion is a group of blocks which can be statically scheduled.
 * Once all inputs are available (t=0), all block execution times are known
 * relative to t. Unlike ControlRegions, ScheduledRegions can contain
 * intermediate dataflow outside of the ScheduledRegion. This allows
 * interaction with shared resources without incurring the cost of dynamically
 * scheduling each and every interaction. The major restriction of external
 * dataflow is that all external dataflow must have constant-time delay. This
 * constant time delay can be guaranteed statically or dynamically.
 *
 * External constant-time delay which is statically scheduled means that the
 * dataflow graph simply cannot backpressure, so it does not rely on data from
 * other data flows and cannot contend with other things.
 *
 * Dynamically guaranteed constant time delay means that a dynamic scheduler is
 * guaranteeing that contention will not occur and necessary data will be
 * available if execution begins at a particular time. This would likely be
 * achieved by delaying execution of this and/or other blocks at certain points
 * in time. Some sort of orchestrator would likely be used. (This support
 * nowhere near available at the time of writing.)
 *
 * Restrictions: ScheduledRegions must obey the NED (no extraneous dependency)
 * and SC (self cleaning) rules, as defined by LI-BDN networks.
 *
 * Note: at the moment, scheduled regions assume synthesis for a synchronous
 * (clocked) system. If asynchronous backends are developed at some point, we
 * may have to re-think the internal operation of scheduled regions.
 *
 * 2nd Note: I expect ScheduledRegions to completely supersede ControlRegions
 * completely at some point.
 *
 *
 * Building ScheduledRegions:
 * Finding valid scheduled regions is a multi-step process. First we begin with
 * one port and grow the region to include any and all ports accessible through
 * AND_FireOne dependences. This represents the maximum possible size of the
 * region, however can lead to violation of various restrictions, such at the
 * NED property and constant-time external dataflow. In the second step, we fix
 * these violations. The first step is executed by running the constructor then
 * the 'grow' method. The second step is executed with the 'finalize' method'.
 */
class ScheduledRegion : public ContainerModule {
public:
    class Cycle {
        friend class ScheduledRegion;

        ScheduledRegion* _sr;
        Cycle*           _prev;
        std::set<OutputPort*> _newValues;
        std::set<const OutputPort*> _available;
        std::set<InputPort*>  _firing;

        std::map<const OutputPort*, PipelineRegister*>
                                    _regs;
        PipelineStageController* _controller;

        Cycle(ScheduledRegion* sr) :
            _sr(sr),
            _prev(nullptr)
        { }

        void finalize(unsigned cycleNum);
        OutputPort* getPipelinedPort(const OutputPort*);

    public:
        DEF_GET_NP(newValues);
        DEF_GET_NP(available);
        DEF_GET_NP(firing);
        DEF_GET_NP(regs);
        DEF_GET_NP(controller);
    };

protected:
    MutableModule* _parent;

    std::set<OutputPort*> _internalOutputs;
    std::set<InputPort*>  _internalInputs;

    /// Set of ports which are scheduled by this region. Only used during the
    //  building steps and cleared afterwards.
    struct Members {
        std::set<const Port*> members;
        std::set<BlockP> memBlockPtrs;

        void shrinkToConstraints(
            const OutputPort* root,
            ConnectionDB* conns);
        void removeIneligiblePorts();
        void findDeps(
            const OutputPort* op,
            ConnectionDB* conns,
            std::set<const Port*>& allDeps,
            std::set<const OutputPort*>& extDeps) const;
        std::set<const OutputPort*> findExtOuts(ConnectionDB* conns) const;
        std::set<const OutputPort*>  findExtIns(ConnectionDB* conns) const;

        auto begin() { return members.begin(); }
        auto end() { return members.end(); }
        auto count(const Port* p) const { return members.count(p); }
        auto size() const { return members.size(); }
        auto insert(const Port* p) {
            memBlockPtrs.insert(p->ownerP());
            return members.insert(p);
        }
        template<typename IT>
        auto insert(IT begin, IT end) {
            for (auto i=begin; i != end; i++) {
                memBlockPtrs.insert((*i)->ownerP());
            }
            return members.insert(begin, end);
        }

        bool contains(const Port* p) const {
            return members.count(p) > 0;
        }

        void erase(const Port* p) {
            members.erase(p);
        }
        void erase(const Block*);
        std::set<const InputPort*> getAllInputs() const;
        void cleanInternal();
    } _members;

    /// Set of blocks which are connected entirely within this region
    std::set<BlockP>      _fullMembers;

    /// Set of ports which are constant.
    std::set<const Port*>  _constPorts;

    std::set<OutputPort*>  _externalOutputs;
    std::set<InputPort*>   _externalInputs;

    /// Wait block to signal start of region
    Wait*                  _startControl;


    /// Has this block been finalized?
    bool                   _finalized;

    /**
     * For each output port, the set of input ports which can be run once it's
     * been fired. These input ports may also be waiting for other output ports
     * to fire, so they can't necessarily run. This many-to-many correspondence
     * is possible because several Waits, Joins, ect. may have been removed
     * from the graph but those semantic dependences must be retained. This
     * data structure maintains them.
    */
    std::map<const OutputPort*, std::set<const InputPort*> > 
        _executesNotLaterThan;
    std::vector<Cycle> _cycles;
    std::map<const Port*, unsigned> _cycleIdx;
    const std::set<const InputPort*>&
        calculateExecutionOrder(const OutputPort*, ConnectionDB*);
    void calculateOrder(ConnectionDB* conns);
    void addNewMembers();
    void scheduleMinimumClocks();
    void finalizeCycles();

    /**
     * Given an internal ("virtual") input port, find the internal ("virtual")
     * output ports upon which it depends and the respective latencies. Only
     * works after "absorb" has been called.
     */
    std::set<OutputPort*> findVirtualDeps(
        const InputPort*,
        std::set<const InputPort*> seen = {} ) const;

    /**
     * Given an input port contained inside of this SR, find all of the
     * external inputs upon which it depends. Only works after "absorb" has
     * been called.
     */
    DependenceRule findInternalDeps(
        const InputPort*,
        std::set<const InputPort*> seen = {} ) const;

    /**
     * Given one of this module's output ports, find all of the external input
     * ports upon which it depends. In a correctly-formed SR, the answer is all
     * of them, but this function determines that the hard way. Only works
     * after "absorb" has been called.
     */
    DependenceRule findInternalDeps(const OutputPort*) const;

    // Find the member input ports upon which this OP depends. Non-recursive.
    DependenceRule internalDeps(const OutputPort*) const;


    /**
     * Find member port which drives a particular input port. This port may be
     * a full member or a virtual member.
     */
    const OutputPort* findInternalSource(const InputPort*) const;

    /**
     * Find member port which is driven by a particular output port. These
     * ports may be full members or virtual members.
     */
    void findInternalSinks(const OutputPort*,
                           std::set<const InputPort*>&) const;


    /// Clean up internal data structures
    void cleanInternal();

    bool add(const Port*);
    void addDriven(const InputPort*);
    void addDrivers(const OutputPort*);

    bool isConnectedToMe(const Port*) const;

    /**
     * Examine the _members of this region and identify external and internal
     * IOs.
     */
    void identifyIO();

    /**
     * Absorb the full member blocks and connections
     */
    void absorb();
    void absorb(OutputPort*, InputPort*);

    /**
     * Check & optimize the interior
     */
    void checkOptFinalize();

public:
    ScheduledRegion(MutableModule* parent,
                    Block* seed,
                    std::string name="",
                    const std::set<const Port*>& constPorts = {});
    virtual ~ScheduledRegion();

    static bool PortAllowed(const Port*);
    static bool BlockAllowedFull(Block* b);
    static bool BlockAllowedVirtual(Block* b);

    bool isConst(const Port* p) const {
        return _constPorts.count(p) > 0;
    }

    DEF_GET_NP(internalOutputs);
    DEF_GET_NP(internalInputs);
    DEF_GET_NP(externalOutputs);
    DEF_GET_NP(externalInputs);
    DEF_ARRAY_GET(cycles);
    DEF_GET_NP(startControl);

    bool grow(const Port*);
    bool grow();

    bool contains(Block* b) {
        return _fullMembers.count(b) > 0;
    }

    /**
     * Finalize the region by pruning it to ensure it meets restrictions, then
     * absorb the member blocks and connections. Ensure that the specified port
     * is retained in the pruning process.
     */
    bool finalize(const OutputPort*);

    virtual void validityCheck() const;
    virtual std::string print() const;

    virtual DependenceRule deps(const OutputPort* op) const;

    virtual bool refine(ConnectionDB& conns) const;
    virtual bool refinable() const {
        return true;
    }

    /// How many clock cycles does it take to compute this?
    unsigned clocks() const;
    unsigned findClockNum(const Port*) const;

    void debugPrint(std::string name) const;

    /// If finalized, don't give out a mutable DB
    virtual ConnectionDB* conns() {
        return _finalized ?
                    nullptr :
                    ContainerModule::conns();
    }

    virtual const ConnectionDB* conns() const {
        return ContainerModule::conns();
    }
};

class FormScheduledRegionPass : public ModulePass {
public:
    FormScheduledRegionPass(Design& d) :
        ModulePass(d) 
    { }

    virtual void runInternal(Module*);
};

}

#endif // __LLPM_SCHEDULED_REGION_HPP__
