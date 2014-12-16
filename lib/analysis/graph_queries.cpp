#include "graph_queries.hpp"

#include <llpm/block.hpp>
#include <analysis/graph.hpp>
#include <analysis/graph_impl.hpp>

using namespace std;

namespace llpm {
namespace queries {

typedef Path<OutputPort, InputPort> CyclePath;
struct CycleDetectionVisitor : public Visitor<CyclePath> {
    bool foundCycle;

    CycleDetectionVisitor() :
        foundCycle(false) {
    }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const CyclePath& path) {
        InputPort* current = path.endPort();
        Block* block = current->owner();
        if (block->hasCycle()) {
            foundCycle = true;
            return TerminateSearch;
        }

        if (path.hasCycle()) {
            // printf("Block %p %s\n", block, block->name().c_str());
            // p2.print();
            foundCycle = true;
            return TerminateSearch;
        } else {
            return Continue;
        }
    }
};

bool BlockCycleExists(const ConnectionDB* conns, vector<OutputPort*> init) {
    CycleDetectionVisitor visitor;
    GraphSearch<CycleDetectionVisitor, DFS> search(conns, visitor);
    search.go(init);
    return visitor.foundCycle;
}


}
}
