#include "transform.hpp"

namespace llpm {

void Transformer::remove(Block* b) {
    if (b->module() != _mod) {
        throw InvalidArgument("This transformer can only operation on blocks in its module");
    }

    auto inputs = b->inputs();
    if (inputs.size() != 1) {
        throw InvalidArgument("This function can only remove blocks with 1 input port");
    }

    auto outputs = b->outputs();
    if (b->outputs().size() > 1) {
        throw InvalidArgument("This function can only remove blocks with 1 output port");
    }

    InputPort* ip = *inputs.begin();
    OutputPort* driver = _conns->findSource(ip);
    _conns->disconnect(driver, ip);

    OutputPort* op = *outputs.begin();
    vector<InputPort*> sinks;
    _conns->findSinks(op, sinks);
    for (InputPort* sink: sinks) {
        _conns->disconnect(op, sink);
        _conns->connect(driver, sink);
    }

    _conns->removeBlock(b);
}

};
