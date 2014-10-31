#include "pipeline.hpp"

namespace llpm {

Pipeline::Pipeline(MutableModule* mod) :
    _module(mod),
    _schedule(NULL)
{ 
    if (mod == NULL)
        throw InvalidArgument("Module argument cannot be NULL!");
}

Pipeline::~Pipeline() {
}

void Pipeline::build() {
    buildMinimum();
}

void Pipeline::buildMinimum() {
    _stages.clear();
    _schedule = _module->schedule();

    const set<Connection>& allConns = _module->conns()->raw();
    for (const Connection& c: allConns) {
        Block* a = c.source()->owner();
        Block* b = c.sink()->owner();
        
        StaticRegion* sra = _schedule->findRegion(a);
        StaticRegion* srb = _schedule->findRegion(b);

        if (sra == srb) {
            // Don't insert a stage if they're in the same region
            _stages[c] = 0;
        } else {
            // Insert a stage if the connection crosses a region
            // boundary
            _stages[c] = 1;
        }
    }
}

} // namespace llpm

