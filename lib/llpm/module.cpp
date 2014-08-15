#include "module.hpp"

namespace llpm {

void ContainerModule::addInputPort(InputPort* ip) {
    _inputMap.insert(make_pair(ip, Identity(ip->type())));
}

void ContainerModule::addOutputPort(OutputPort* op) {
    _outputMap.insert(make_pair(op, Identity(op->type())));
}

bool ContainerModule::refine(std::vector<Block*>& blocks,
                             std::map<InputPort*, vector<InputPort*> >& ipMap,
                             std::map<OutputPort*, OutputPort*>& opMap) const
{
    this->blocks(blocks);

    BOOST_FOREACH(InputPort* ip, _inputs) {
        auto driver = getDriver(ip);
        _conns.findSinks(driver, ipMap[ip]);
    }

    BOOST_FOREACH(OutputPort* op, _outputs) {
        auto sink = getSink(op);
        opMap[op] = _conns.findSource(sink);
    }

    return true;
}

bool ContainerModule::internalRefine() {
    assert(false && "Not implemented!");
    return false;
}

} // namespace llpm
