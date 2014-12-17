#include "control_region.hpp"

#include <analysis/graph.hpp>

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

    set<ControlRegion*> regions;
    set<Block*> seen;
    deque<InputPort*> crInputs;

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

        if (dynamic_cast<ControlRegion*>(b) != NULL ||
            dynamic_cast<DummyBlock*>(b) != NULL) {
            // It's already a control region? Cool
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
        cr->grow();
        regions.insert(cr);
        crInputs.insert(crInputs.end(),
                        cr->inputs().begin(), cr->inputs().end());
        counter++;       
    }
    
    printf("Formed %lu control regions\n", regions.size());
}

bool ControlRegion::grow() {
    bool ret = false;

    // This loop will probably only run once to grow and fail the second time,
    // but maybe not...
    while (true) {
        auto gu = growUp();
        auto gd = growDown();
        if (gu || gd) {
            ret = true;
        } else {
            break;
        }
    }

    return ret;
}

bool ControlRegion::growUp() {
    ConnectionDB* pdb = _parent->conns();
    for (InputPort* ip: this->inputs()) {
        OutputPort* driver = pdb->findSource(ip);
        if (driver)
            if (add(driver->owner()))
                return true;
    }
    return false;
}

bool ControlRegion::growDown() {
    ConnectionDB* pdb = _parent->conns();
    for (OutputPort* op: this->outputs()) {
        vector<InputPort*> sinks;
        pdb->findSinks(op, sinks);
        for (auto sink: sinks)
            if (add(sink->owner()))
                return true;
    }
    return false;
}

bool ControlRegion::canGrow(InputPort* ip) {
    auto driver = getDriver(ip);
    vector<InputPort*> sinks;
    _conns.findSinks(driver, sinks);
    for (auto sink: sinks) {
        if (sink->owner()->firing() == OR)
            return false;
    }
    return true;
}

bool ControlRegion::canGrow(OutputPort* op) {
    auto sink = getSink(op);
    auto driver = _conns.findSource(sink);
    return !(driver && driver->owner()->outputsIndependent());
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

bool ControlRegion::add(Block* b) {
    ConnectionDB* pdb = _parent->conns();

    if (dynamic_cast<Module*>(b) != NULL ||
        dynamic_cast<DummyBlock*>(b) != NULL) {
        // Don't merge in unrefined modules
        // or dummy I/O blocks
        return false;
    }

    if (b->hasCycle())
        // Don't allow cycles
        return false;

    map<InputPort*, OutputPort*> internalizedOutputs;
    map<OutputPort*, set<InputPort*> > internalizedInputs;
    if (_conns.raw().size() != 0) {
        // Make sure the block is adjacent to this CR
        for (InputPort* ip: b->inputs()) {
            OutputPort* oldSource = pdb->findSource(ip);
            if (oldSource &&
                oldSource->owner() == this &&
                canGrow(oldSource))
                internalizedOutputs[ip] = oldSource;
        } 

        for (OutputPort* op: b->outputs()) {
            vector<InputPort*> oldSinks;
            pdb->findSinks(op, oldSinks);
            for (auto ip: oldSinks)
                if (ip->owner() == this &&
                    canGrow(ip))
                    internalizedInputs[op].insert(ip);
        }

        /**
         * Maintain the constraints:
         */
        bool foundAsSink = internalizedOutputs.size() > 0;
        bool foundAsDriver = internalizedInputs.size() > 0;

        // Must be adjacent
        if (!foundAsDriver && !foundAsSink) {
            // printf("not adj\n");
            return false;
        }

        // Must not create additional valid bits within CR
        if (foundAsDriver && !foundAsSink && b->outputsIndependent()) {
            // printf("is router\n");
            return false;
        }

        // Must not create additional back pressure bits with CR
        if (foundAsSink && !foundAsDriver && b->firing() == OR) {
            // printf("is select\n");
            return false;
        }
    }

    bool internallyDriven = false;
    for (OutputPort* op: b->outputs()) {

        auto f = internalizedInputs.find(op);
        if (f != internalizedInputs.end()) {
            internallyDriven = true;
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

        auto newIP = addInputPort(ip);
        if (oldSource)
            pdb->connect(newIP, oldSource);
    }

    return true;
}

typedef Edge<InputPort, OutputPort> OIEdge;

struct OIEdgeVisitor : public Visitor<OIEdge> {
    set<InputPort*> deps;
    map<OutputPort*, InputPort*> inputDrivers;

    OIEdgeVisitor(const ContainerModule* cm) {
        for (auto ip: cm->inputs())
            inputDrivers[cm->getDriver(ip)] = ip;
    }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const OIEdge& edge) {
        OutputPort* current = edge.endPort();
        auto f = inputDrivers.find(current);
        if (f != inputDrivers.end())
            deps.insert(f->second);
        return Continue;
    }
};

set<InputPort*> ControlRegion::findDependences(OutputPort* op) const {
    OIEdgeVisitor visitor(this);
    GraphSearch<OIEdgeVisitor, DFS> search(&_conns, visitor);
    search.go(std::vector<InputPort*>({getSink(op)}));
    return visitor.deps;
}

void ControlRegion::validityCheck() const {
    ContainerModule::validityCheck();

    // NO cycles!
    assert(!hasCycle());

    // Check to make sure all outputs have the same deps as the module as a
    // whole
    set<InputPort*> cannonDeps =
        set<InputPort*>(inputs().begin(), inputs().end());
    for (OutputPort* op: outputs()) {
        auto deps = findDependences(op);
        assert(cannonDeps == deps &&
               "Error: control region may introduce a deadlock. "
               "This is a known problem, but was not expected to occur "
               "in practice. Please report this problem!");
    }
}

} // namespace llpm
