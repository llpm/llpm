#ifndef __LLPM_SCHEDULED_REGION_HPP__
#define __LLPM_SCHEDULED_REGION_HPP__

#include <llpm/module.hpp>
#include <passes/pass.hpp>

namespace llpm {

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
protected:
    MutableModule* _parent;

    std::set<OutputPort*> _internalOutputs;
    std::set<InputPort*>  _internalInputs;

    /// Set of ports which are scheduled by this region. Only used during the
    //  building steps and cleared afterwards.
    std::set<const Port*> _members;

    /// Set of ports external to this region which are scheduled by this region
    //  (virtual region members).
    std::set<Port*>       _vmembers;

    /// Set of blocks which are connected entirely within this region
    std::set<Block*>      _fullMembers;

    std::set<OutputPort*> _externalOutputs;
    std::set<InputPort*>  _externalInputs;

    bool add(const Port*, const std::set<const Port*>& constPorts = {});
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

    /**
     * Check & optimize the interior
     */
    void checkOptFinalize();

public:
    ScheduledRegion(MutableModule* parent,
                    Block* seed,
                    std::string name="");
    virtual ~ScheduledRegion();

    static bool BlockAllowedFull(Block* b);
    static bool BlockAllowedVirtual(Block* b);

    DEF_GET_NP(internalOutputs);
    DEF_GET_NP(internalInputs);
    DEF_GET_NP(externalOutputs);
    DEF_GET_NP(externalInputs);

    bool grow(const Port*, const std::set<const Port*>& constPorts = {});
    bool grow(const std::set<const Port*>& constPorts = {});
    /**
     * Finalize the region by pruning it to ensure it meets restrictions, then
     * absorb the member blocks and connections. Ensure that the specified port
     * is retained in the pruning process.
     */
    void finalize(Port*);

    virtual void validityCheck() const;
    virtual std::string print() const;

    virtual DependenceRule deps(const OutputPort* op) const;

    virtual bool refine(ConnectionDB& conns) const;
    virtual bool refinable() const {
        return true;
    }

    /// How many clock cycles does it take to compute this?
    unsigned clocks();
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
