#include "graph_queries.hpp"

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <libraries/core/interface.hpp>
#include <analysis/graph.hpp>
#include <analysis/graph_impl.hpp>

#include <boost/range/adaptor/reversed.hpp>

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


typedef Path<InputPort, OutputPort> IOPath;
struct TokenAnalysisVisitor : public Visitor<IOPath> {
    const Port* source;
    bool foundSource;
    bool foundOR;
    bool foundCycle;

    map<InputPort*, bool> requiresSource;

    TokenAnalysisVisitor(const Port* source) :
        source(source),
        foundSource(false),
        foundOR(false),
        foundCycle(false)
    { }

    // TODO: I'm not sure that this method of checking whether or not blocks
    // (output ports) along a path require the source to fire is correct.
    // Write some Thorough unit tests!
    void addSource(const IOPath& path) {
        bool requires = false;
        for (const auto& edge: boost::adaptors::reverse(path.raw())) {
            if (edge.first == source || edge.second == source) {
                requiresSource[edge.first] = true;
                requires = true;
            } else {
                OutputPort* op = edge.second;
                auto dr = op->depRule();
                auto deps = op->deps();
                // Assumption: requiresSource[x] defaults to 'false' when key
                // x is not an entry
                if (dr.inputType() == DependenceRule::AND) {
                    for (auto dep: deps) {
                        requires |= requiresSource[dep];
                    }
                } else {
                    for (auto dep: deps) {
                        requires &= requiresSource[dep];
                    }
                }
            }
        }
    }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB* conns,
                    const IOPath& path) {
        OutputPort* current = path.endPort();
        Block* block = current->owner();
        auto pathCycle = path.hasCycle();
        if (block->hasCycle() || pathCycle()) {
            foundCycle = true;
        }

        auto dr = current->depRule();
        if (dr.inputType() != DependenceRule::AND) {
            // Include both OR and Custom
            foundOR = true;
        }

        if (current == source || source == path.end().first) {
            foundSource = true;
            addSource(path);
            return TerminatePath;
        } else {
            if (pathCycle)
                return TerminatePath;
            else
                return Continue;
        }
    }

    // Find the src ports exposed by the vertex we are visiting.
    Terminate next(const ConnectionDB* conns,
                   const IOPath& path, 
                   std::vector<typename PathTy::SrcPortTy*>& nextvec) {
        OutputPort* current = path.endPort();
        if (conns->isInternalDriver(current)) {
            addSource(path);
            return TerminatePath;
        }

        auto deps = current->deps();
        nextvec.insert(nextvec.end(), deps.begin(), deps.end());
        return Continue;
   }
};

bool TokenOrderAnalysis(
        Port* source, Port* sink,
        bool& singleSource, bool& reorderPotential, bool& cyclic) {
    if (source->owner()->module() != sink->owner()->module()) {
        throw InvalidArgument(
            "Sink and source must be within the same module!");
    }

    ConnectionDB* conns = source->owner()->module()->conns();
    if (conns == NULL) {
        throw InvalidArgument("Can only analyze transparent modules!");
    }

    vector<InputPort*> init;
    InputPort* sinkIP = sink->asInput();
    if (sinkIP != NULL) {
        init.push_back(sinkIP);
    } else {
        auto sinkOP = sink->asOutput();
        conns->findSinks(sinkOP, init);
    }

    TokenAnalysisVisitor visitor(source);
    GraphSearch<TokenAnalysisVisitor, DFS> search(conns, visitor);
    search.go(init);

    singleSource = visitor.requiresSource[init.front()];
    reorderPotential = visitor.foundOR;
    cyclic = visitor.foundCycle;
    return visitor.foundSource;
}

bool CouldReorderTokens(Interface* iface) {
    // There are two conditions to check here:
    //  1) Does there exist more than one control path from request to
    //  response? If so, should they have different latencies than reordering
    //  is possible.
    //  2) Does the response port have an independent (OR) dependence on
    //  anything other than the request? If so, than additional tokens can be
    //  injected.

    bool singleSource, reorderPotential, cyclic;
    bool rc = TokenOrderAnalysis(iface->req(), iface->resp(), 
                                 singleSource, reorderPotential, cyclic);
    if (!rc)
        throw InvalidArgument(
            "This interface does not appear to be properly connected as an "
            "interface! (It's responses are not driven by the reqs.)");
    return singleSource && !reorderPotential;
}

} // namespace queries
} // namespace llpm
