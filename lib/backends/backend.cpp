#include "backend.hpp"

namespace llpm {

Time Backend::latency(InputPort* ip, OutputPort* op) const {
    assert(ip->owner() == op->owner());
    // Drastic oversimplification: estimate the latency as the
    // "logical effort" in nanoseconds
    return Time::ns(ip->owner()->logicalEffort(ip, op));
}

} // namespace llpm
