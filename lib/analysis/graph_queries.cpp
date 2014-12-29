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


typedef Edge<InputPort, OutputPort> IOEdge;
struct DominatorVisitor : public Visitor<IOEdge> {
    std::set<Block*> dominators;

    Terminate visit(const ConnectionDB*,
                    const IOEdge& edge) {
        dominators.insert(edge.endPort()->owner());
        return Continue;
    }
};

void FindDominators(const ConnectionDB* conns,
                    Block* b,
                    std::set<Block*>& dominators) {
    DominatorVisitor visitor;
    GraphSearch<DominatorVisitor, DFS> search(conns, visitor);
    search.go(b->inputs());
    dominators.swap(visitor.dominators);
}

bool CouldReorderTokens(Interface* iface) {
    // FIXME: This is very clearly wrong!
    return false;
}

} // namespace queries
} // namespace llpm
