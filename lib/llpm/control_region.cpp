#include "control_region.hpp"

#include <analysis/graph.hpp>
#include <analysis/graph_queries.hpp>
#include <libraries/synthesis/fork.hpp>

#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <algorithm>
#include <deque>

using namespace std;

namespace llpm {

void FormControlRegionPass::runInternal(Module* mod) {
    if (dynamic_cast<ControlRegion*>(mod) != NULL)
        return;

    ContainerModule* cm = dynamic_cast<ContainerModule*>(mod);
    if (cm == NULL)
        return;

    printf("Forming module into control regions...\n");

    ConnectionDB* conns = cm->conns();
    unsigned counter = 1;

    set<Block*> seen;
    deque<InputPort*> crInputs;

    // Ports which are driven by const values. Can always be included
    // or even cloned
    set<Port*> constPorts;
    // set<Block*> constBlocks;
    // queries::FindConstants(mod, constPorts, constBlocks);

    // Start with outputs
    for (OutputPort* op: cm->outputs()) {
        InputPort* opSink = cm->getSink(op);
        assert(opSink != NULL);
        crInputs.push_back(opSink);
    }
    
    while (crInputs.size() > 0) {
        InputPort* ip = crInputs.front();
        crInputs.pop_front();

        OutputPort* opDriver = conns->findSource(ip);
        if (opDriver == NULL)
            // An un-driven input? Odd, but whatever...
            continue;

        Block* b = opDriver->owner();
        if (b->module() != mod)
            // This block has been relocated since it was put in the list
            continue;

        if (b->is<Module>() ||
            b->is<DummyBlock>() ||
            !ControlRegion::BlockAllowed(b)) {
            // It's a dummy block? Don't mess with it
            // But, we have to find its drivers to continue the search
            if (seen.count(b) == 0) {
                crInputs.insert(crInputs.end(),
                                b->inputs().begin(), b->inputs().end());
                seen.insert(b);
            }
            continue;
        }

        std::string crName = str(boost::format("%1%_cr%2%") 
                                    % mod->name()
                                    % counter);
        ControlRegion* cr = new ControlRegion(cm, opDriver->owner(), crName);
        cr->grow(constPorts);
        crInputs.insert(crInputs.end(),
                        cr->inputs().begin(), cr->inputs().end());
        counter++;
    }
    

    unsigned regions = 0;
    unsigned flattened = 0;
    set<Block*> allBlocks;
    conns->findAllBlocks(allBlocks);
    for (auto b: allBlocks) {
        auto cr = dynamic_cast<ControlRegion*>(b);
        if (cr != NULL) {
            regions++;
            if (cr->size() <= 1) {
                // No need for a CR with one block in it!
                cr->refine(*conns);
                flattened++;
            } else {
                cr->finalize();
            }
        }
    }

    printf("Formed %u control regions\n", regions);
    printf("Flattened %u CRs with one internal block\n", flattened);
}

bool ControlRegion::BlockAllowed(Block* b) {
    if (!b->outputsTied() ||
         b->firing() != DependenceRule::AND) {
        return false;
    }
    return true;
}

bool ControlRegion::grow(const std::set<Port*>& constPorts) {
    bool ret = false;

    // This loop will probably only run once to grow and fail the second time,
    // but maybe not...
    while (true) {
        auto gu = growUp(constPorts);
        auto gd = growDown(constPorts);
        if (gu || gd) {
            ret = true;
        } else {
            break;
        }
    }

    return ret;
}

bool ControlRegion::growUp(const std::set<Port*>& constPorts) {
    ConnectionDB* pdb = _parent->conns();
    for (InputPort* ip: this->inputs()) {
        OutputPort* driver = pdb->findSource(ip);
        if (driver)
            if (add(driver->owner(), constPorts))
                return true;
    }
    return false;
}

bool ControlRegion::growDown(const std::set<Port*>& constPorts) {
    ConnectionDB* pdb = _parent->conns();
    for (OutputPort* op: this->outputs()) {
        vector<InputPort*> sinks;
        pdb->findSinks(op, sinks);
        for (auto sink: sinks)
            if (add(sink->owner(), constPorts))
                return true;
    }
    return false;
}

bool ControlRegion::canGrow(InputPort* ip) {
    auto driver = getDriver(ip);
    vector<InputPort*> sinks;
    _conns.findSinks(driver, sinks);
    for (auto sink: sinks) {
        if (sink->owner()->firing() != DependenceRule::AND)
            return false;
    }
    return true;
}

bool ControlRegion::canGrow(OutputPort* op) {
    auto sink = getSink(op);
    auto driver = _conns.findSource(sink);
    return driver && driver->owner()->outputsTied();
}

bool ControlRegion::canGrow(Port* p) {
    auto op = dynamic_cast<OutputPort*>(p);
    if (op)
        return canGrow(op);

    auto ip = dynamic_cast<InputPort*>(p);
    if (ip)
        return canGrow(ip);

    throw InvalidArgument("What in the hell sort of port did you pass me?!");
}

bool ControlRegion::add(Block* b, const std::set<Port*>&) {
    ConnectionDB* pdb = _parent->conns();

    if (b->is<Module>() && b->isnot<ControlRegion>())
        // Don't merge in unrefined modules
        return false;

    if (pdb->isblacklisted(b))
        // Don't merge my parent's dummy I/O blocks
        return false;

    if (b->hasCycle())
        // Don't allow cycles
        return false;

    if (!BlockAllowed(b))
        return false;

    map<OutputPort*, InputPort*> existingInputValues;
    for (auto ip: inputs()) {
        auto driver = pdb->findSource(ip);
        if (driver)
            existingInputValues[driver] = ip;
    }

    map<InputPort*, OutputPort*> internalizedOutputs;
    map<OutputPort*, set<InputPort*> > internalizedInputs;
    if (_conns.numConnections() != 0) {
        /* Make sure this block is valid for inclusion */
        
        // First, determine if it would add a new input
        bool createsInput = false;
        for (InputPort* ip: b->inputs()) {
            OutputPort* oldSource = pdb->findSource(ip);
            if (oldSource &&
                oldSource->owner() == this &&
                canGrow(oldSource))
                internalizedOutputs[ip] = oldSource;
            else if (existingInputValues.find(oldSource) ==
                         existingInputValues.end())
                createsInput = true;
        }

        // Determine if it would add a new Output
        bool createsOutput = false;
        for (OutputPort* op: b->outputs()) {
            vector<InputPort*> oldSinks;
            pdb->findSinks(op, oldSinks);
            for (auto ip: oldSinks)
                if (ip->owner() == this &&
                    canGrow(ip))
                    internalizedInputs[op].insert(ip);
                else
                    createsOutput = true;
        }

        /**
         * Maintain the constraints:
         */
        bool foundAsSink = internalizedOutputs.size() > 0;
        bool foundAsDriver = internalizedInputs.size() > 0;

#if 0
        printf("%s <- %s, crI: %u, crO: %u, iO: %lu, iI: %lu, "
               "fS: %u, fD: %u\n",
               b->globalName().c_str(), this->name().c_str(),
               createsInput, createsOutput, internalizedOutputs.size(),
               internalizedInputs.size(), foundAsSink, foundAsDriver);
#endif

        /* Now check a bunch of conditions based on the above
         * analysis */

        // Must be adjacent
        if (!foundAsDriver && !foundAsSink) {
            // printf("not adj\n");
            return false;
        }

        if (createsInput && !foundAsDriver)
            // This would break DEP
            return false;

        if (createsOutput && !foundAsSink)
            // This would break DEP
            return false;

        // Must not create additional valid bits within CR
        if (foundAsDriver && !foundAsSink && !b->outputsTied()) {
            // printf("is router\n");
            return false;
        }

        // Must not create additional back pressure bits with CR
        if (foundAsSink && !foundAsDriver &&
            b->firing() != DependenceRule::AND) {
            // printf("is select\n");
            return false;
        }
    }

    if (b->is<ControlRegion>()) {
        auto bmConns = b->module()->conns();
        if (bmConns != NULL) {
            // Get rid of the CR to allow us to absorb its contents
            // printf("Breaking %s for %s\n",
                   // b->name().c_str(), name().c_str());
            b->refine(*bmConns);
            return true;
        }
        return false;
    }

    for (OutputPort* op: b->outputs()) {

        auto f = internalizedInputs.find(op);
        if (f != internalizedInputs.end()) {
            // Special case: outputs drive us, so connect internally
            for (InputPort* ip: f->second) {
                pdb->disconnect(f->first, ip);
                auto internalDriver = getDriver(ip);
                _conns.remap(internalDriver, op);
                removeInputPort(ip);
            }
        }

        // Find remaining old sinks
        vector<InputPort*> oldSinks;
        pdb->findSinks(op, oldSinks);
        for (auto ip: oldSinks)
            pdb->disconnect(op, ip);

        OutputPort* newOP = NULL;
        for (auto ip: oldSinks) {
            if (newOP == NULL)
                newOP = addOutputPort(op);
            pdb->connect(ip, newOP);
        }
    }

    for (InputPort* ip: b->inputs()) {
        auto f = internalizedOutputs.find(ip);
        if (f != internalizedOutputs.end()) {
            // Special case: merging block connected to this guy, so just
            // bring the connection internal
            assert(ip == f->first);
            InputPort* internalSink = getSink(f->second);
            OutputPort* internalDriver = _conns.findSource(internalSink);

            if (!_conns.createsCycle(Connection(internalDriver, f->first))) {
                pdb->disconnect(f->first, f->second);
                _conns.connect(internalDriver, f->first);

                vector<InputPort*> remainingExtSinks;
                pdb->findSinks(f->second, remainingExtSinks);
                if (remainingExtSinks.size() == 0)
                    removeOutputPort(f->second);
                continue;
            }
        }


        // Make a new input port
        OutputPort* oldSource = pdb->findSource(ip);
        if (oldSource)
            pdb->disconnect(ip, oldSource);

        auto existingF = existingInputValues.find(oldSource);
        if (existingF == existingInputValues.end()) {
            // New input value, so create a new input port
            auto newIP = addInputPort(ip);
            if (oldSource)
                pdb->connect(newIP, oldSource);
        } else {
            // Just tie it to the existing port
            auto intDriver = getDriver(existingF->second);
            _conns.connect(intDriver, ip);
        }
    }

    return true;
}

typedef Edge<InputPort, OutputPort> IOEdge;

struct IOEdgeVisitor : public Visitor<IOEdge> {
    set<InputPort*> deps;
    map<OutputPort*, InputPort*> inputDrivers;

    IOEdgeVisitor(const ContainerModule* cm) {
        for (auto ip: cm->inputs())
            inputDrivers[cm->getDriver(ip)] = ip;
    }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const IOEdge& edge) {
        OutputPort* current = edge.endPort();
        auto f = inputDrivers.find(current);
        if (f != inputDrivers.end())
            deps.insert(f->second);
        return Continue;
    }
};

set<InputPort*> ControlRegion::findDependences(OutputPort* op) const {
    IOEdgeVisitor visitor(this);
    GraphSearch<IOEdgeVisitor, DFS> search(&_conns, visitor);
    search.go(std::vector<InputPort*>({getSink(op)}));
    return visitor.deps;
}

void ControlRegion::validityCheck() const {
    ContainerModule::validityCheck();

    // NO cycles!
    assert(!hasCycle());

    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (auto b: blocks) {
        assert(b->firing() == DependenceRule::AND);
        assert(b->outputsTied());
        assert(!b->outputsSeparate());
    }
}

void ControlRegion::finalize() {
    // Check to make sure all outputs have the same deps as the module as a
    // whole
    set<InputPort*> cannonDeps =
        set<InputPort*>(inputs().begin(), inputs().end());
    for (OutputPort* op: outputs()) {
        auto deps = findDependences(op);
        assert(cannonDeps == deps &&
               "Error: control region may introduce a deadlock.");
    }

    // Eliminate Waits and Forks
    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (auto b: blocks) {
        if (b->is<Wait>()) {
            auto w = b->as<Wait>();
            OutputPort* source = _conns.findSource(w->din());
            vector<InputPort*> sinks;
            _conns.findSinks(w->dout(), sinks);
            for (auto sink: sinks) {
                _conns.disconnect(w->dout(), sink);
                if (source != NULL)
                    _conns.connect(source, sink);
            }
            _conns.removeBlock(w);
        }

        if (b->is<Fork>()) {
            auto f = b->as<Fork>();
            OutputPort* source = _conns.findSource(f->din());
            for (unsigned i=0; i<f->dout_size(); i++) {
                vector<InputPort*> sinks;
                _conns.findSinks(f->dout(i), sinks);
                for (auto sink: sinks) {
                    _conns.disconnect(f->dout(i), sink);
                    if (source != NULL)
                        _conns.connect(source, sink);
                }
            }
            _conns.removeBlock(f);
        }
    }

    this->unifyOutput();
    this->_finalized = true;
}

bool ControlRegion::refine(ConnectionDB& conns) const {
    if (!_finalized)
        return ContainerModule::refine(conns);

    assert(false &&
           "CR Refinement not written. Needs to add Wait and Forks.");
    return false;
}


const std::vector<InputPort*>&
    ControlRegion::deps(const OutputPort*) const {
    return inputs();
}

DependenceRule ControlRegion::depRule(const OutputPort*) const {
    return DependenceRule(DependenceRule::AND, DependenceRule::Always);
}


} // namespace llpm
