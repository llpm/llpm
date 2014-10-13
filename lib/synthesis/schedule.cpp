#include "schedule.hpp"
#include <llpm/module.hpp>
#include <synthesis/pipeline.hpp>

#include <deque>

namespace llpm {

Schedule::Schedule(Module* mod) :
    _module(dynamic_cast<MutableModule*>(mod))
{
    if (mod == NULL)
        throw InvalidArgument("Module argument cannot be NULL");
}

Schedule::~Schedule() {
    for (auto sr: _regions) {
        delete sr;
    }
}

void Schedule::buildBaseSchedule() {
    vector<Block*> blocks;
    _module->blocks(blocks);
    _module->submodules(blocks);

    unsigned idCounter = 1;
    for(Block* b: blocks) {
        StaticRegion* sr = new StaticRegion(idCounter++, this, b);
        _regions.push_back(sr);
    }

    buildBlockMap();
}

void Schedule::buildBlockMap() {
    _blockMap.clear();

    for (StaticRegion* sr: _regions) {
        for (Block* b: sr->blocks()) {
            _blockMap[b] = sr;
        }
    }
}

void StaticRegion::schedule(Pipeline* pipeline, vector<Layer>& layers) {
    map< Block*, set<Connection> > revDeps;
    map< Block*, set<Connection> > outputs;
    map< Block*, unsigned > depCount;
    map<Block*, unsigned> layerAssignment;

    MutableModule* mod = _schedule->module();
    ConnectionDB* conns = mod->conns();
    for (Block* b: _blocks) {
        std::vector<Connection> bConns;
        conns->find(b, bConns);

        unsigned numDeps = 0;
        for (Connection c: bConns) {
            switch (classifyConnection(c)) {
            case Internal:
                if (c.source()->owner() == b)
                    revDeps[b].insert(c);
                else
                    numDeps += 1;
                break;
            case Output:
                outputs[b].insert(c);
                break;
            case Input:
                break;
            }
        }

        depCount[b] = numDeps;
        layerAssignment[b] = 0;
    }
   
    set<Block*> blocksRemaining = _blocks;
    while (blocksRemaining.size() > 0) {
        deque<Block*> schedNow;
        for (Block* b: blocksRemaining) {
            if (depCount[b] == 0) {
                schedNow.push_back(b);
            }
        }

        if (schedNow.size() == 0) {
            throw ImplementationError("Could not find block to schedule. Is there a cycle in the static region?");
        }

        for (Block* b: schedNow) {
            blocksRemaining.erase(b);

            unsigned layerNum = layerAssignment[b];
            if (layers.size() <= layerNum)
                layers.resize(layerNum + 1);
            Layer& layer = layers[layerNum];
            layer.addBlock(b);

            auto rdeps = revDeps[b];
            for (Connection c: rdeps) {
                Block* rdep = c.sink()->owner();
                unsigned& dc = depCount[rdep];
                assert(dc > 0);
                dc -= 1;

                unsigned& depLayerNum = layerAssignment[b];
                unsigned addLatency = pipeline->stages(c);
                unsigned newLayerNum = layerNum + addLatency;
                if (depLayerNum < newLayerNum)
                    depLayerNum = newLayerNum;
            }
        }
    }
}

StaticRegion::ConnType StaticRegion::classifyConnection(Connection c) {
    Block* sinkBlock = c.sink()->owner();
    Block* sourceBlock = c.source()->owner();
    auto sinkLcl = _blocks.count(sinkBlock) > 0;
    auto sourceLcl = _blocks.count(sourceBlock) > 0;

    if (sinkLcl && sourceLcl) {
        return Internal;
    } else if (sinkLcl) {
        return Input;
    } else if (sourceLcl) {
        return Output;
    } else {
        assert(false && "This case should not occur!");
    }
}

} // namespace llpm

