#ifndef __LLPM_SYNTHESIS_SCHEDULE_HPP__
#define __LLPM_SYNTHESIS_SCHEDULE_HPP__

#include <cassert>
#include <set>
#include <unordered_map>

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/module.hpp>

namespace llpm {

// fwd defs. C++ is dumb ++
class Pipeline;
class Schedule;

class StaticRegion {
public:
    class Layer {
        friend class StaticRegion;

        vector<Block*> _blocks;
        vector<Connection> _outputs;

        void addBlock(Block* b) {
            _blocks.push_back(b);
        }

        void addOutputEdge(Connection c) {
            _outputs.push_back(c);
        }

    public:
        DEF_GET_NP(blocks);
    };

    enum ConnType {
        Input,
        Internal,
        Output
    };

private:
    Schedule*   _schedule;
    set<Block*> _blocks;

public:
    StaticRegion(Schedule* s, Block* b):
        _schedule(s) {
        _blocks.insert(b);
    }

    void add(StaticRegion& a) {
        _blocks.insert(a._blocks.begin(), a._blocks.end());
    }

    void add(Block* b) {
        _blocks.insert(b);
    }

    const set<Block*>& blocks() {
        return _blocks;
    }

    void schedule(Pipeline*, vector<Layer>&);
    ConnType classifyConnection(Connection conn);
};

/*
 * LI Schedule. Contains a list of regions (sets of blocks) which
 * can be statically scheduled in terms of LI properties (valid bits
 * and back pressure).
 */
class Schedule {
    MutableModule* _module;
    set<StaticRegion*> _regions;
    unordered_map<Block*, StaticRegion*> _blockMap;

    void buildBlockMap();

public:
    Schedule(Module* module);
    virtual ~Schedule();

    DEF_GET_NP(module);

    void buildBaseSchedule();

    const set<StaticRegion*>& regions() {
        return _regions;
    }

    StaticRegion* findRegion(Block* b) const {
        auto f = _blockMap.find(b);
        if (f != _blockMap.end())
            return f->second;
        if (b->module() != _module)
            throw InvalidArgument("Block does not belong to the same module as this schedule!");
        throw ImplementationError("Could not find region for block. Does schedule have to be re-built?");
    }
};

} // namespace llpm

#endif // __LLPM_SYNTHESIS_SCHEDULE_HPP__
