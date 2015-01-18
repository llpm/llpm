#ifndef __BACKENDS_BACKEND_HPP__
#define __BACKENDS_BACKEND_HPP__

#include <llpm/block.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class Backend {
protected:
    Backend() { }

public:
    virtual ~Backend() { }

    virtual bool blockIsPrimitive(Block* b) = 0;
    virtual Refinery::StopCondition* primitiveStops() = 0;
};

};

#endif // __BACKENDS_BACKEND_HPP__
