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

StaticRegion* Schedule::createModuleIORegion() {
    auto sr = new StaticRegion(_regions.size(), this, StaticRegion::IO);
    _regions.push_back(sr);
    return sr;
}

StaticRegion* Schedule::createSpecialRegion() {
    auto sr = new StaticRegion(_regions.size(), this, StaticRegion::Special);
    _regions.push_back(sr);
    return sr;
}

void Schedule::buildSchedule() {
    buildBaseSchedule();
}

void Schedule::buildBaseSchedule() {
    vector<Block*> blocks;
    _module->blocks(blocks);
    _module->submodules(blocks);

    buildBlockMap();

    unsigned idCounter = _regions.size();
    for(Block* b: blocks) {
        if (_blockMap.find(b) == _blockMap.end()) {
            StaticRegion* sr = new StaticRegion(idCounter++, this, b);
            _regions.push_back(sr);
        }
    }

    buildBlockMap();
}

void Schedule::buildBlockMap() {
    _blockMap.clear();

    for (StaticRegion* sr: _regions) {
        buildBlockMap(sr);
    }
}

void Schedule::buildBlockMap(StaticRegion* sr) {
    for (Block* b: sr->blocks()) {
        _blockMap[b] = sr;
    }
}

void StaticRegion::schedule(Pipeline* pipeline) {
    assert(this->_scheduled == false);
    ConnectionDB* conns = _schedule->module()->conns();
    deque<Block*> toAdd;
    for (Block* b: _blocks) {
        for (InputPort* ip: b->inputs()) {
            Connection c;
            auto found = conns->find(ip, c);
            if (!found)
                continue;

            unsigned stages = pipeline->stages(c);
            if (stages > 0) {
                OutputPort* op = c.source();
                conns->disconnect(c);
                for (unsigned i=0; i<stages; i++) {
                    PipelineRegister* reg = new PipelineRegister(op);
                    conns->connect(op, reg->din());
                    op = reg->dout();
                    toAdd.push_back(reg);
                }
                conns->connect(op, ip);

                auto foundV = conns->find(ip, c);
                assert(foundV);
            }
        }
    }
    this->_blocks.insert(toAdd.begin(), toAdd.end());
    this->_schedule->buildBlockMap(this);
    this->_scheduled = true;
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

