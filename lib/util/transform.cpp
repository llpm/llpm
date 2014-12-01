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
    if (driver)
        _conns->disconnect(driver, ip);

    OutputPort* op = *outputs.begin();
    vector<InputPort*> sinks;
    _conns->findSinks(op, sinks);
    for (InputPort* sink: sinks) {
        _conns->disconnect(op, sink);
        if (driver)
            _conns->connect(driver, sink);
    }

    _conns->removeBlock(b);
}

void Transformer::replace(Block* b, Block* with) {
    if (b->module() != _mod) {
        throw InvalidArgument("This transformer can only operation on blocks in its module");
    }

    auto inputs = b->inputs();
    if (inputs.size() == with->inputs().size()) {
        throw InvalidArgument("This function can only remove blocks with 1 input port");
    }

    auto outputs = b->outputs();
    if (outputs.size() == with->outputs().size()) {
        throw InvalidArgument("This function can only remove blocks with 1 output port");
    }

    auto numInputs = inputs.size();
    auto biIter = inputs.begin();
    auto wiIter = with->inputs().begin();
    for (unsigned i = 0; i < numInputs; i++) {
        InputPort* ip = *biIter;
        OutputPort* driver = _conns->findSource(ip);
        if (driver) {
            _conns->disconnect(driver, ip);
            _conns->connect(driver, *wiIter);
        }

        biIter++;
        wiIter++;
    }

    auto numOutputs = outputs.size();
    auto boIter = outputs.begin();
    auto woIter = with->outputs().begin();
    for (unsigned i = 0; i < numOutputs; i++) {
        OutputPort* op = *boIter;
        vector<InputPort*> sinks;
        _conns->findSinks(op, sinks);
        for (auto sink: sinks) {
            _conns->disconnect(op, sink);
            _conns->connect(*woIter, sink);
        }

        boIter++;
        woIter++;
    }

    _conns->removeBlock(b);
}

};
