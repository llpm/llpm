#ifndef __BACKENDS_BACKEND_HPP__
#define __BACKENDS_BACKEND_HPP__

#include <llpm/block.hpp>
#include <refinery/refinery.hpp>
#include <util/files.hpp>

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

    virtual void writeModule(FileSet& dir,
                             Module* mod,
                             std::set<FileSet::File*>& files) = 0;
};

};

#endif // __BACKENDS_BACKEND_HPP__
