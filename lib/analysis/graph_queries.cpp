#include "graph_queries.hpp"

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <libraries/core/interface.hpp>
#include <libraries/core/logic_intr.hpp>
#include <analysis/graph.hpp>
#include <analysis/graph_impl.hpp>

#include <boost/range/adaptor/reversed.hpp>

using namespace std;

namespace llpm {
namespace queries {

typedef Path<OutputPort, InputPort> CyclePath;
struct CycleDetectionVisitor : public Visitor<CyclePath> {
    set< std::pair<OutputPort*, InputPort*> > seen;
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
        } else if (seen.count(path.end()) > 0) {
            return TerminatePath;
        } else {
            seen.insert(path.end());
            return Continue;
        }
    }
};

bool BlockCycleExists(const ConnectionDB* conns,
                      const set<OutputPort*>& initList) {
    for (auto init: initList) {
        CycleDetectionVisitor visitor;
        GraphSearch<CycleDetectionVisitor, DFS> search(conns, visitor);
        vector<OutputPort*> op({init});
        search.go(op);
        if (visitor.foundCycle)
            return true;
    }
    return false;
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
        if (block->hasCycle() || pathCycle) {
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

typedef Path<OutputPort, InputPort> CyclePath;
struct CycleFindingVisitor: public Visitor<CyclePath> {
    vector< std::pair<OutputPort*, InputPort*> > cycle;
    set< std::pair<OutputPort*, InputPort*> > seen;
    boost::function<bool(Block*)> ignoreBlock;

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const CyclePath& path) {
        if (ignoreBlock(path.endPort()->owner())) {
            // Don't visit this connection
            return TerminatePath;
        } else if (path.hasCycle()) {
            this->cycle = path.extractCycle();
            return TerminateSearch;
        } else if (seen.count(path.end())) {
            return TerminatePath;
        } else {
            seen.insert(path.end());
            return Continue;
        }
    }
};

bool FindCycle(Module* mod,
               boost::function<bool(Block*)> ignoreBlock,
               std::vector< Connection >& cycle) {
    CycleFindingVisitor visitor;
    visitor.ignoreBlock = ignoreBlock;
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        throw InvalidArgument("Cannot analyze opaque module!");
    GraphSearch<CycleFindingVisitor, DFS> search(conns, visitor);
    vector<OutputPort*> init;
    mod->internalDrivers(init);
    search.go(init);

    cycle.resize(visitor.cycle.size());
    for (unsigned j=0; j<visitor.cycle.size(); j++) {
        cycle[j] = visitor.cycle[j];
    }

    return cycle.size() > 0;
}

struct ConstFindingVisitor : public Visitor<OIEdge> {
    set<Block*> constBlocks;
    set<Port*> constPorts;
    set<InputPort*> seenIP;

    map<Block*, unsigned> numConstInputs;

    void addBlock(Block* b) {
        constBlocks.insert(b);
        constPorts.insert(b->outputs().begin(),
                          b->outputs().end());
    }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const OIEdge& edge)
    {
        assert(constPorts.count(edge.end().first) > 0);
        constPorts.insert(edge.endPort());
        assert(seenIP.count(edge.endPort()) == 0);
        seenIP.insert(edge.endPort());

        // Increment counter for number of const inputs
        Block* b = edge.endPort()->owner();
        unsigned& nc = numConstInputs[b];
        assert(nc < b->inputs().size());
        nc++;

        if (nc == b->inputs().size()) {
            addBlock(b);
            return Continue;
        } else {
            return TerminatePath;
        }
    }

};

// Find all edges and blocks which are driven entirely by Constants
void FindConstants(Module* mod,
                   std::set<Port*>& constPorts,
                   std::set<Block*>& constBlocks)
{
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        throw InvalidArgument("Cannot analyze opaque module!");

    ConstFindingVisitor visitor;
    GraphSearch<ConstFindingVisitor, BFS> search(conns, visitor);
    vector<OutputPort*> init;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (Block* b: blocks) {
        Constant* c = dynamic_cast<Constant*>(b);
        if (c != NULL) {
            visitor.addBlock(c);
            for (auto op: c->outputs())
                init.push_back(op);
        }
    }
    printf("Starting with %lu const ports\n", visitor.constPorts.size());
    search.go(init);
    printf("Ending with %lu const ports\n", visitor.constPorts.size());

    if (constBlocks.size() == 0)
        constBlocks.swap(visitor.constBlocks);
    else
        constBlocks.insert(visitor.constBlocks.begin(),
                           visitor.constBlocks.end());

    if (constPorts.size() == 0)
        constPorts.swap(visitor.constPorts);
    else
        constPorts.insert(visitor.constPorts.begin(),
                           visitor.constPorts.end());

}

struct DepFindingVisitor : public Visitor<IOEdge> {
    bool first = true;
    DependenceRule rule;
    set<OutputPort*> deps;

    Terminate visit(const ConnectionDB*,
                    const PathTy& path)
    {
        if (first) {
            rule = path.endPort()->depRule();
            deps.clear();
            first = false;
        } else {
            rule = rule + path.endPort()->depRule();
        }
        return Continue;
    }

    Terminate pathEnd(const ConnectionDB*,
                      const PathTy& path)
    {
        deps.insert(path.endPort());
        return Continue;
    }
};

void FindDependencies(const Module* mod,
                      InputPort* ip,
                      std::set<OutputPort*>& deps,
                      DependenceRule& rule)
{
    const ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        throw InvalidArgument("Cannot analyze opaque module!");

    DepFindingVisitor visitor;
    GraphSearch<DepFindingVisitor, DFS> search(conns, visitor);
    search.go(vector<InputPort*>{ip});
    deps.swap(visitor.deps);
    rule = visitor.rule;
}



} // namespace queries
} // namespace llpm
