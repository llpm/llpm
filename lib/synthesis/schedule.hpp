#ifndef __LLPM_SYNTHESIS_SCHEDULE_HPP__
#define __LLPM_SYNTHESIS_SCHEDULE_HPP__

#include <cassert>
#include <set>
#include <list>
#include <unordered_map>
#include <unordered_set>

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/module.hpp>

#include <libraries/synthesis/pipeline.hpp>

namespace llpm {

// fwd defs. C++ is dumb ++
class Pipeline;
class Schedule;

/* A StaticRegion represents a region of blocks which can be
 * statically scheduled. In other words, it is a feed-forward region
 * of blocks, though it may not be implemented that way.
 */
class StaticRegion {
public:
#if 0
    class Layer {
        friend class StaticRegion;

        set<Block*> _blocks;

        Layer() {
            _controller = new PipelineStageController();
        }
        
    public:
        void addBlock(Block* b) {
            _blocks.insert(b);
        }

        template<typename T>
        void addBlocks(T cntr) {
            for (auto b: cntr)
                addBlock(b);
        }
        DEF_GET_NP(blocks);
        DEF_GET_NP(controller);

        bool contains(Block* b) const {
            return _blocks.find(b) != _blocks.end();
        }

        void buildRegisters();
    };
#endif

    enum ConnType {
        Input,
        Internal,
        Output
    };

    enum Type {
        Normal,
        Special,
        IO
    };

private:
    unsigned _id;
    Type _type;
    Schedule*   _schedule;
    set<Block*> _blocks;
    bool _scheduled;

public:
    StaticRegion(unsigned id, Schedule* s, Block* b, Type type = Normal):
        _id(id),
        _type(type),
        _schedule(s), 
        _scheduled(false) {
        _blocks.insert(b);
    }

    StaticRegion(unsigned id, Schedule* s, Type type = Normal):
        _id(id),
        _type(type),
        _schedule(s) { }

    DEF_GET_NP(id);
    DEF_GET_NP(type);

    void add(StaticRegion& a) {
        _blocks.insert(a._blocks.begin(), a._blocks.end());
    }

    void add(Block* b) {
        _blocks.insert(b);
    }

    const set<Block*>& blocks() {
        return _blocks;
    }

    void findIO(set<InputPort*>& inputs,
                set<OutputPort*>& outputs);

    void schedule(Pipeline*);
    ConnType classifyConnection(Connection conn);
};

/*
 * LI Schedule. Contains a list of regions (sets of blocks) which
 * can be statically scheduled in terms of LI properties (valid bits
 * and back pressure).
 */
class Schedule {
    friend class StaticRegion;

    MutableModule* _module;
    list<StaticRegion*> _regions;
    unordered_map<Block*, StaticRegion*> _blockMap;

    void buildBlockMap();
    void buildBlockMap(StaticRegion* r);

public:
    Schedule(Module* module);
    virtual ~Schedule();

    DEF_GET_NP(module);

    void buildSchedule();
    void buildBaseSchedule();

    const list<StaticRegion*>& regions() {
        return _regions;
    }

    StaticRegion* createModuleIORegion();
    StaticRegion* createSpecialRegion();

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
