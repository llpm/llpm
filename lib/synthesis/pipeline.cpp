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
    vector< set<Connection> > unorderedCycles;
    map<Connection, unsigned> occurrences;
   
    {
        vector< vector<Connection> > cycles;
        queries::FindAllCycles(_module, cycles);

        for (auto&& cycle: cycles) {
            unorderedCycles.emplace_back(cycle.begin(), cycle.end());
            for (auto&& conn: cycle)
                occurrences[conn] += 1;
        }
    }

    while (!unorderedCycles.empty()) {
        // Find the most common edge (connection)
        Connection maxC;
        unsigned   maxV = 0;
        for (auto op: occurrences) {
            if (!op.first.source()->pipelineable())
                continue;
            if (op.second > maxV) {
                maxC = op.first;
                maxV = op.second;
            }
        }

        // maxC is the connection with the most occurrences. Pipeline it.
        _stages[maxC] = 1;

        // Remove all cycles which contained maxC -- they are broken
        for (unsigned i=0; i<unorderedCycles.size(); i++) {
            auto& cycle = unorderedCycles[i];
            if (cycle.count(maxC)) {
                for (auto& conn: cycle) {
                    auto& o = occurrences[conn];
                    assert( o > 0 );
                    o -= 1;
                    if (o == 0)
                        occurrences.erase(conn);
                }
                unorderedCycles.erase(unorderedCycles.begin() + i);
            }
        }
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

