#include "backend.hpp"

#include <llpm/control_region.hpp>

using namespace std;

namespace llpm {

Time Backend::latency(InputPort* ip, OutputPort* op) const {
    assert(ip->owner() == op->owner());
    // Drastic oversimplification: estimate the latency as the
    // "logical effort" in nanoseconds
    return Time::ns(ip->owner()->logicalEffort(ip, op));
}

Time Backend::maxLatency(OutputPort* op) const {
    const vector<InputPort*>& inputs = op->deps();
    Time maxT;
    for (auto ip: inputs) {
        Time t = latency(ip, op);
        if (t > maxT)
            maxT = t;
    }
    return maxT;
}

Time Backend::latency(Connection c) const {
    auto source = c.source()->owner();
    auto sink = c.sink()->owner();
    if ((source->is<Module>() && !source->is<ControlRegion>()) ||
        (sink->is<Module>() && !source->is<ControlRegion>()))
    {
        // Naive assumption: If we are connecting a module to another module,
        // they are spaced far apart so we have a high latency. In the case, a
        // ControlRegion does not count as a module.
        return Time::ns(10);
    }
    // Assume that everything else is a relatively local connection and thus
    // relatively short latency
    return Time::ps(250);

    // TODO: if both source and/or sink are pipeline registers, look through
    // them and get the latency, then divide by the total number of registers
    // in the chain.
}

} // namespace llpm
