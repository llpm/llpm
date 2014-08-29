#include "schedule.hpp"
#include <llpm/module.hpp>

namespace llpm {

Schedule::Schedule(Module* mod) :
    _module(mod)
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

    for(Block* b: blocks) {
        StaticRegion* sr = new StaticRegion(b);
        _regions.insert(sr);
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

} // namespace llpm

