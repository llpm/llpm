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

InputPort* ContainerModule::addInputPort(InputPort* ip, std::string name) {
    if (name == "")
        name = str(boost::format("input%1%") % inputs().size());
    auto dummy = new DummyBlock(ip->type());
    InputPort* extIp = new InputPort(this, ip->type(), name);
    _inputMap.insert(make_pair(extIp, dummy));
    dummy->name(name + "_dummy");
    conns()->blacklist(dummy);
    this->conns()->connect(dummy->dout(), ip);
    return extIp;
}

OutputPort* ContainerModule::addOutputPort(OutputPort* op, std::string name) {
    if (name == "")
        name = str(boost::format("output%1%") % outputs().size());
    auto dummy = new DummyBlock(op->type());
    OutputPort* extOp = new OutputPort(this, op->type(), name);
    _outputMap.insert(make_pair(extOp, dummy));
    dummy->name(name + "_dummy");
    conns()->blacklist(dummy);
    this->conns()->connect(op, dummy->din());
    return extOp;
}

bool ContainerModule::refine(ConnectionDB& conns) const
{
    conns.update(_conns);

    for(InputPort* ip: _inputs) {
        auto f = _inputMap.find(ip);
        assert(f != _inputMap.end());
        conns.remap(ip, f->second->din());
    }

    for(OutputPort* op: _outputs) {
        auto f = _outputMap.find(op);
        assert(f != _outputMap.end());
        conns.remap(op, f->second->dout());
    }

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
