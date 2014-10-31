#ifndef __LLPM_SYNTHESIS_PIPELINE_HPP__
#define __LLPM_SYNTHESIS_PIPELINE_HPP__

#include <cassert>
#include <map>

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/module.hpp>
#include <refinery/priority_collection.hpp>
#include <synthesis/schedule.hpp>

namespace llpm {

class Pipeline {
    MutableModule* _module;
    Schedule* _schedule;

    std::map<Connection, unsigned> _stages;

public:
    Pipeline(MutableModule* mod);
    virtual ~Pipeline();

    // Returns the number of pipeline stages to be inserted in
    // on a particular connection between ports
    unsigned stages(Connection c) const {
        auto f = _stages.find(c);
        if (f != _stages.end())
            return f->second;
        if (_module->conns()->exists(c))
            throw ImplementationError("Could not find stages entry. Has the pipeline database been built?");
        throw InvalidArgument("Could not find stages entry because connection does not exist!");
    }

    // Builds pipeline stages only where absolutely necessary
    void buildMinimum();

    void build();
};

} // namespace llpm

#endif // __LLPM_SYNTHESIS_PIPELINE_HPP__
