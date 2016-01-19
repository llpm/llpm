#include "backend.hpp"

#include <llpm/control_region.hpp>

using namespace std;

namespace llpm {

Latency Backend::latency(const InputPort* ip, const OutputPort* op) const {
    assert(ip->owner() == op->owner());
    // Drastic oversimplification: estimate the latency as the
    // "logical effort" in nanoseconds
    auto t = Time::ns(ip->owner()->logicalEffort(ip, op));

    auto dr = op->deps();
    assert(dr.inputs.size() == dr.latencies.size());
    for (unsigned i=0; i<dr.inputs.size(); i++) {
        if (dr.inputs[i] == ip) {
            auto l = dr.latencies[i];
            if (l.time().unknown()) {
                return Latency(t, l.depth());
            } else {
                return l;
            }
        }
    }
    assert(false && "IP not found!");
}

Latency Backend::maxTime(const OutputPort* op) const {
    const auto& inputs = op->deps().inputs;
    Latency maxT;
    for (auto ip: inputs) {
        auto t = latency(ip, op);
        if (t.depth() > maxT.depth())
            maxT = t;
    }
    return maxT;
}

Latency Backend::latency(Connection c) const {
    auto source = c.source()->owner();
    auto sink = c.sink()->owner();
    if ((source->is<Module>() && !source->is<ControlRegion>()) ||
        (sink->is<Module>() && !source->is<ControlRegion>()))
    {
        // Naive assumption: If we are connecting a module to another module,
        // they are spaced far apart so we have a high latency. In the case, a
        // ControlRegion does not count as a module.
        return Latency(Time::ns(10), PipelineDepth::Fixed(0));
    }

    if (source->maxLogicalEffort(c.source()) == 0.0) {
        // If the source block does nothing, assume it will be synthesized away
        // along with its output routing.
        return Latency(Time::ns(0), PipelineDepth::Fixed(0));
    }
    // Assume that everything else is a relatively local connection and thus
    // relatively short latency
    return Latency(Time::ps(250), PipelineDepth::Fixed(0));

    // TODO: if both source and/or sink are pipeline registers, look through
    // them and get the latency, then divide by the total number of registers
    // in the chain.
}

} // namespace llpm
