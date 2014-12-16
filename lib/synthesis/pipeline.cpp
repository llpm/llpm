#include "pipeline.hpp"

#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>

using namespace std;

namespace llpm {

Pipeline::Pipeline(MutableModule* mod) :
    _module(mod)
{ 
    if (mod == NULL)
        throw InvalidArgument("Module argument cannot be NULL!");
}

Pipeline::~Pipeline() {
}

void Pipeline::build() {
    buildMinimum();
    insertPipelineRegs();
}

static bool is_module(Block* b) {
    return dynamic_cast<Module*>(b) != NULL;
}

static bool is_dummy(Block* b) {
    return dynamic_cast<DummyBlock*>(b) != NULL;
}

void Pipeline::buildMinimum() {
    _stages.clear();

    const set<Connection>& allConns = _module->conns()->raw();
    for (const Connection& c: allConns) {
        Block* a = c.source()->owner();
        Block* b = c.sink()->owner();

        if (is_module(a) || is_module(b)) {
            // Connections between modules and other things generally need to
            // be pipelined 
            _stages[c] = 1;

            if (is_dummy(a) || is_dummy(b))
                // Exception: module outputs can be passed through
                _stages[c] = 0;
        } else if (dynamic_cast<ControlRegion*>(_module)) {
            // If we are pipelining a control region, connections can avoid
            // pipelining.
            _stages[c] = 0;
        } else {
            // Conservatively make everything else pipelined
            _stages[c] = 1;
        }
    }
}

void Pipeline::insertPipelineRegs() {
    ConnectionDB* conns = _module->conns();
    set<Connection> raw = conns->raw();
    for (Connection c: raw) {
        unsigned stages = this->stages(c);
        if (stages > 0) {
            OutputPort* op = c.source();
            InputPort*  ip = c.sink();
            conns->disconnect(c);
            for (unsigned i=0; i<stages; i++) {
                PipelineRegister* reg = new PipelineRegister(op);
                conns->connect(op, reg->din());
                op = reg->dout();
            }
            conns->connect(op, ip);
        }
    }
}

} // namespace llpm

