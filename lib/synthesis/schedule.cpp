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
    // Find specially treat nodes
    vector<Block*> blocks;
    this->module()->blocks(blocks);
    this->module()->submodules(blocks);

    for(Block* b: blocks) {
        if (findRegion(b) != NULL)
            continue;

        Select* sel = dynamic_cast<Select*>(b);
        if (sel != NULL) {
            auto sr = createSpecialRegion();
            sr->add(sel);
        }

        Router* rtr = dynamic_cast<Router*>(b);
        if (rtr != NULL) {
            auto sr = createSpecialRegion();
            sr->add(rtr);
        }
    }

    buildBlockMap();
    buildGreedySchedule();
    buildBaseSchedule();
}

void Schedule::dfsGreedyBuild(Block* b, set<Block*> path,
                              StaticRegion* sr, set<Block*>& discard) {
    path.insert(b);
    for (OutputPort* source: b->outputs()) {
        vector<InputPort*> sinks;
        _module->conns()->findSinks(source, sinks);
        for (InputPort* sink: sinks) {
            Block* owner = sink->owner();
            if (findRegion(owner) == NULL) {
                if (path.count(owner) == 0 &&
                    !owner->hasCycles()) {
                    sr->add(owner);
                    dfsGreedyBuild(owner, path, sr, discard);
                } else {
#if 0
                    cout << "discard insert "
                        <<_module->design().namer().primBlockName(owner)
                        << endl;
#endif
                    discard.insert(owner);
                }
            } else {
#if 0
                cout << "Already grouped "
                        <<_module->design().namer().primBlockName(owner)
                        << endl;
#endif
            }
        }
    }
}

void Schedule::buildGreedySchedule() {
    set<Block*> discard;
    ConnectionDB* conns = _module->conns();
#if 0
    for (InputPort* modInput: _module->inputs()) {
        auto driver = _module->getDriver(modInput);
        vector<InputPort*> sinks;
        conns->findSinks(driver, sinks);
        for (auto sink: sinks) {
            discard.insert(sink->owner());
        }
    }
#endif

    for (auto sr: _regions) {
        for (auto b: sr->blocks()) {
            for (auto op: b->outputs()) {
                vector<InputPort*> sinks;
                conns->findSinks(op, sinks);
                for (auto sink: sinks) {
                    discard.insert(sink->owner());
                }
            }
        }
    }

    while (!discard.empty()) {
        Block* b = *discard.begin();
        discard.erase(b);
        if (findRegion(b))
            continue;

        StaticRegion* sr = new StaticRegion(_regions.size(), this, b);
        _regions.push_back(sr);
#if 0
        cout << "Greedy build "
                <<_module->design().namer().primBlockName(b)
                << endl;
#endif
        dfsGreedyBuild(b, set<Block*>(), sr, discard);
        for (auto&& srBlock: sr->blocks()) {
            _blockMap[srBlock] = sr;
        }
    }
}

void Schedule::buildBaseSchedule() {
    vector<Block*> blocks;
    _module->blocks(blocks);
    _module->submodules(blocks);

    buildBlockMap();

    unsigned idCounter = _regions.size();
    for(Block* b: blocks) {
        if (findRegion(b) == NULL) {
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
    if (this->_scheduled)
        return;
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

