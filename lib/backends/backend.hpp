#ifndef __BACKENDS_BACKEND_HPP__
#define __BACKENDS_BACKEND_HPP__

#include <llpm/block.hpp>
#include <refinery/refinery.hpp>
#include <llpm/connection.hpp>
#include <util/files.hpp>
#include <util/time.hpp>

namespace llpm {
// Fwd defs, not very fwd looking
class Design;

class Backend {
protected:
    Design& _design;

    Backend(Design& design) :
        _design(design) { }

public:
    virtual ~Backend() { }

    virtual bool blockIsPrimitive(Block* b) = 0;
    virtual Refinery::StopCondition* primitiveStops() = 0;

    /**
     * What is the maximum latency from an input port to an output port in the
     * same block? Ideally, this returns an accurate time based on layout
     * information, but an estimate is more realistic.
     */
    virtual Time latency(const InputPort*, const OutputPort*) const;

    /**
     * What is the longest latency to this output port?
     */
    Time maxLatency(const OutputPort*) const;

    /**
     * What is the maximum latency for a signal to propagate a connection?
     * Again, this is ideally based on layout or at least floor planning, but
     * that may not be practical.
     */
    virtual Time latency(Connection) const;

    /**
     * Are we compiling for a synchronous (clocked) execution substrate?
     */
    virtual bool synchronous() const = 0;

    virtual void writeModule(FileSet& dir,
                             Module* mod,
                             std::set<FileSet::File*>& files) = 0;
};

};

#endif // __BACKENDS_BACKEND_HPP__
