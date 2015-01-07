#include "pipeline.hpp"

#include <llpm/control_region.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <analysis/graph_queries.hpp>

#include <set>

using namespace std;

namespace llpm {

Pipeline::Pipeline(MutableModule* mod) :
    _module(mod),
    _built(false)
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

void Pipeline::buildMinimum() {
    _stages.clear();

    // Strictly speaking, pipeline registers are only necessary for
    // _correctness_ when there exists a cycle in the graph. Therefore,
    // the _minimum_ pipelining is found by locating graph cycles and breaking
    // them with pipeline regs. We can further reduce pipeline regs by
    // breaking them starting with the most common edges in cycles --
    // breaking those edges simultaneously breaks the most cycles!
    //
    set<Connection> broken;
    vector<Connection> cycle;
    while (queries::FindCycle(_module, broken, cycle)) {
        Connection toBreak = cycle.back();
        assert(broken.count(toBreak) == 0);
        broken.insert(toBreak);
        assert(_stages[toBreak] == 0);
        _stages[toBreak] = 1;
    }
    _built = true;
}

void Pipeline::insertPipelineRegs() {
    ConnectionDB* conns = _module->conns();
    vector<Connection> connections(conns->begin(), conns->end());
    for (const auto& c: connections) {
        unsigned stages = this->stages(c);
        if (stages > 0) {
            OutputPort* op = c.source();
            assert(op->pipelineable());
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

