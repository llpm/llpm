#include "module.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>
#include <util/graph.hpp>

#include <boost/format.hpp>

#include <deque>

namespace llpm {

ContainerModule::~ContainerModule() {
    if (_schedule)
        delete _schedule;

    if (_pipeline)
        delete _pipeline;
}

void ContainerModule::addInputPort(InputPort* ip) {
    auto dummy = new DummyBlock(ip->type());
    _inputMap.insert(make_pair(dummy->din(), dummy));
    _inputs.push_back(dummy->din());
    dummy->name(str(boost::format("input%1%_dummy") % inputs().size()));
    conns()->blacklist(dummy);
    this->conns()->connect(dummy->dout(), ip);
}

void ContainerModule::addOutputPort(OutputPort* op) {
    auto dummy = new DummyBlock(op->type());
    _outputMap.insert(make_pair(dummy->dout(), dummy));
    _outputs.push_back(dummy->dout());
    dummy->name(str(boost::format("output%1%_dummy") % outputs().size()));
    conns()->blacklist(dummy);
    this->conns()->connect(op, dummy->din());
}

bool ContainerModule::refine(ConnectionDB& conns) const
{
    assert(false && "Not implemented");

#if 0
    for(InputPort* ip: _inputs) {
        auto driver = getDriver(ip);
        _conns.
        _conns.findSinks(driver, ipMap[ip]);
    }

    for(OutputPort* op: _outputs) {
        auto sink = getSink(op);
        opMap[op] = _conns.findSource(sink);
    }

#endif
    return true;
}

unsigned ContainerModule::internalRefine(Refinery::StopCondition* sc) {
    vector<Block*> blocksTmp;
    blocks(blocksTmp);

    auto passes = design().refinery().refine(blocksTmp, _conns, sc);
    return passes;
}

Schedule* ContainerModule::schedule() {
    if (_schedule == NULL) {
        _schedule = new Schedule(this);

        // Add dummy I/O blocks to special IO region
        for (auto p: _inputMap)
            _schedule->createModuleIORegion()->add(p.second);
        for (auto p: _outputMap)
            _schedule->createModuleIORegion()->add(p.second);
    }
    return _schedule;
}

Pipeline* ContainerModule::pipeline() {
    if (_pipeline == NULL) {
        _pipeline = new Pipeline(this);
    }
    return _pipeline;
}

void ContainerModule::validityCheck() const {
    for (InputPort* ip: _inputs) {
        assert(_inputMap.count(ip) == 1);
    }

    for (OutputPort* op: _outputs) {
        assert(_outputMap.count(op) == 1);
    }

    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (Block* b: blocks) {
        assert(b->module() == this);
    }
}

bool ContainerModule::hasCycles() const {
    for (auto p: _inputMap) {
        auto dummy = p.second;
        set<Block*> path;
        if (dfs_cycle(dummy, path, &_conns))
            return true;
    }
    return false;
}

} // namespace llpm
