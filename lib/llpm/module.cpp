#include "module.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>

namespace llpm {

ContainerModule::~ContainerModule() {
    if (_schedule)
        delete _schedule;

    if (_pipeline)
        delete _pipeline;
}

void ContainerModule::addInputPort(InputPort* ip) {
    _inputMap.insert(make_pair(ip, Identity(ip->type())));
}

void ContainerModule::addOutputPort(OutputPort* op) {
    _outputMap.insert(make_pair(op, Identity(op->type())));
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
    vector<Block*> blocksTmp(_blocks.begin(), _blocks.end());

    auto passes = design().refinery().refine(blocksTmp, _conns, sc);

    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    _blocks.clear();
    for (Block* b: blocks) {
        addBlock(b);
    }
    return passes;
}

Schedule* ContainerModule::schedule() {
    if (_schedule == NULL) {
        _schedule = new Schedule(this);
        _schedule->buildBaseSchedule();
    }
    return _schedule;
}

Pipeline* ContainerModule::pipeline() {
    if (_pipeline == NULL) {
        _pipeline = new Pipeline(this);
        _pipeline->buildMinimum();
    }
    return _pipeline;
}

void ContainerModule::validityCheck() const {
    for (Block* b: _blocks) {
        assert(b->module() == this);
    }

    for (Module* b: _modules) {
        assert(b->module() == this);
    }

    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (Block* b: blocks) {
        auto c1 = _blocks.count(b);

        Module* m = dynamic_cast<Module*>(b);
        if (m) {
            auto c2 = _modules.count(m);
            assert( c1 == 0 );
            assert( c2 > 0 );
        } else {
            assert( c1 > 0 );
        }
    }
    // printf("Blocks: %lu, _blocks: %lu, _modules: %lu\n",
           // blocks.size(), _blocks.size(), _modules.size());
    assert(blocks.size() == (_blocks.size() + _modules.size()) );
}

} // namespace llpm
