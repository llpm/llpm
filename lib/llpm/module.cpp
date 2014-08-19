#include "module.hpp"

namespace llpm {

void ContainerModule::addInputPort(InputPort* ip) {
    _inputMap.insert(make_pair(ip, Identity(ip->type())));
}

void ContainerModule::addOutputPort(OutputPort* op) {
    _outputMap.insert(make_pair(op, Identity(op->type())));
}

bool ContainerModule::refine(std::vector<Block*>& blocks,
                             ConnectionDB& conns) const
{
    this->blocks(blocks);
    assert(false && "Not implemented");

#if 0
    BOOST_FOREACH(InputPort* ip, _inputs) {
        auto driver = getDriver(ip);
        _conns.
        _conns.findSinks(driver, ipMap[ip]);
    }

    BOOST_FOREACH(OutputPort* op, _outputs) {
        auto sink = getSink(op);
        opMap[op] = _conns.findSource(sink);
    }

#endif
    return true;
}

unsigned ContainerModule::internalRefine(Design::Refinery::StopCondition* sc) {
    vector<Block*> blocks(_blocks.begin(), _blocks.end());
    auto passes = design().refinery().refine(blocks, _conns, sc);
    _blocks = set<Block*>(blocks.begin(), blocks.end());
    return passes;
}

} // namespace llpm
