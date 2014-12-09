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
    auto dummy = new DummyBlock(ip->type());
    InputPort* extIp = new InputPort(this, ip->type(), ip->name());
    _inputMap.insert(make_pair(extIp, dummy));
    if (name == "")
        dummy->name(str(boost::format("input%1%_dummy") % inputs().size()));
    else
        dummy->name(name);
    conns()->blacklist(dummy);
    this->conns()->connect(dummy->dout(), ip);
    return extIp;
}

OutputPort* ContainerModule::addOutputPort(OutputPort* op, std::string name) {
    auto dummy = new DummyBlock(op->type());
    OutputPort* extOp = new OutputPort(this, op->type(), op->name());
    _outputMap.insert(make_pair(extOp, dummy));
    if (name == "")
        dummy->name(str(boost::format("output%1%_dummy") % outputs().size()));
    else
        dummy->name(name);
    conns()->blacklist(dummy);
    this->conns()->connect(op, dummy->din());
    return extOp;
}

#if 0
bool ContainerModule::refine(ConnectionDB& conns) const
{
    return false;
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
#endif

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
