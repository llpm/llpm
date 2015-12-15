#include "scheduled_region.hpp"

#include <libraries/synthesis/pipeline.hpp>
#include <passes/transforms/simplify.hpp>
#include <analysis/graph_queries.hpp>
#include <libraries/synthesis/fork.hpp>
#include <libraries/synthesis/pipeline.hpp>
#include <libraries/core/logic_intr.hpp>

#include <util/misc.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <boost/format.hpp>

using namespace std;

namespace llpm {

void FormScheduledRegionPass::runInternal(Module* mod) {
    if (dynamic_cast<ScheduledRegion*>(mod) != NULL)
        return;

    if (!mod->design().backend()->synchronous())
        return;

    ContainerModule* cm = dynamic_cast<ContainerModule*>(mod);
    if (cm == NULL)
        return;

    printf("Forming module %s into scheduled regions...\n", 
           mod->name().c_str());

    ConnectionDB* conns = cm->conns();
    unsigned counter = 1;

    set<Block*> seen;
    deque<InputPort*> inputsToSee;

    // Ports which are driven by const values. Can always be included
    // or even cloned
    set<const Port*> constPorts;
    set<Block*> constBlocks;
    queries::FindConstants(mod, constPorts, constBlocks);

    // Start with outputs
    for (OutputPort* op: cm->outputs()) {
        InputPort* opSink = cm->getSink(op);
        assert(opSink != NULL);
        inputsToSee.push_back(opSink);
    }

    while (inputsToSee.size() > 0) {
        InputPort* ip = inputsToSee.front();
        inputsToSee.pop_front();

        OutputPort* opDriver = conns->findSource(ip);
        if (opDriver == NULL)
            // An un-driven input? Odd, but whatever...
            continue;

        Block* b = opDriver->owner();
        if (b->module() != mod)
            // This block has been relocated since it was put in the list
            continue;

        if (!ScheduledRegion::BlockAllowedFull(b)) {
            // It's a dummy block? Don't mess with it
            // But, we have to find its drivers to continue the search
            if (seen.count(b) == 0) {
                inputsToSee.insert(inputsToSee.end(),
                                   b->inputs().begin(), b->inputs().end());
                seen.insert(b);
            }
        } else {
            std::string crName = str(boost::format("%1%_sr%2%") 
                                        % mod->name()
                                        % counter);
            ScheduledRegion* sr =
                new ScheduledRegion(cm, b, crName);
            sr->grow(constPorts);
            if (sr->finalize(opDriver)) {
                const auto ext = sr->inputs();
                inputsToSee.insert(inputsToSee.end(),
                                   ext.begin(), ext.end());
                counter++;
            } else {
                inputsToSee.insert(inputsToSee.end(),
                                   b->inputs().begin(), b->inputs().end());
                seen.insert(b);
            }
        }
    }

    unsigned regions = 0;
    unsigned flattened = 0;
    set<Block*> allBlocks;
    conns->findAllBlocks(allBlocks);
    for (auto b: allBlocks) {
        auto sr = dynamic_cast<ScheduledRegion*>(b);
        if (sr != NULL) {
            regions++;
            if (sr->size() <= 1) {
                // No need for an SR with one block in it!
                // sr->refine(*conns);
                flattened++;
            }
        }
    }

    printf("Formed %u scheduled regions\n", regions);
    printf("Flattened %u SRs with one internal block\n", flattened);
}

bool ScheduledRegion::PortAllowed(const Port* p) {
    if (p->isInput()) {
        // Don't allow members which are not AND_FireOne (input case)
        for (auto op: p->asInput()->findDriven()) {
            auto dr = op->deps();
            if (dr.depType != DependenceRule::AND_FireOne) {
                return false;
            }
        }
        return true;
    } else if (p->isOutput()) {
        auto output = p->asOutput();
        auto dr = output->deps();
        if (dr.depType != DependenceRule::AND_FireOne)
            return false;
        for (auto lat: dr.latencies) {
            // Only allow fixed latency
            if (!lat.depth().fixed() ||
                !lat.depth().finite())
                return false;
        }
        return true;
    }

    assert(false);
}

bool ScheduledRegion::BlockAllowedFull(Block* b) {
    if (!b->outputsTied())
        return false;

    for (auto output: b->outputs()) {
        if (!PortAllowed(output))
            return false;
    }

    if (b->is<Module>() ||
        b->is<DummyBlock>())
        return false;

    if (b->hasCycle())
        return false;

    return true;
}

bool ScheduledRegion::BlockAllowedVirtual(Block* b) {
    if (b->hasCycle())
        return false;

    if (b->is<DummyBlock>())
        return false;

    return true;
}

ScheduledRegion::ScheduledRegion(MutableModule* parent,
                                 Block* seed,
                                 std::string name) :
    ContainerModule(parent->design(), name),
    _parent(parent) {

    assert(BlockAllowedFull(seed));
    module(parent);

    for (auto input: seed->inputs()) {
        _members.insert(input);
    }

    for (auto output: seed->outputs()) {
        _members.insert(output);
    }
}

ScheduledRegion::~ScheduledRegion() { }

bool ScheduledRegion::grow(const Port* port,
                           const std::set<const Port*>& constPorts) {
    assert(_members.count(port) > 0);

    ConnectionDB* conns = port->owner()->module()->conns();
    assert(conns != nullptr);

    if (port->isInput()) {
        auto source = conns->findSource(port->asInput());    
        if (source == nullptr)
            return false;
        return add(source, constPorts);
    } else if (port->isOutput()) {
        std::set<InputPort*> sinks;
        conns->findSinks(port->asOutput(), sinks);
        bool ret = false;
        for (auto sink: sinks) {
            auto rc = add(sink, constPorts);
            ret = ret || rc;
        }
        return ret;
    }
    assert(false);
}

bool ScheduledRegion::grow(const std::set<const Port*>& constPorts) {
    bool ret = false;

    // Keep growing up and down until we can't grow up and down anymore
    while (true) {
        bool grew = false;

        for (auto port: _members) {
            auto rc = grow(port, constPorts);
            grew = grew || rc;
        }

        if (!grew)
            break;
    }
    return ret;
}

bool ScheduledRegion::isConnectedToMe(const Port* port) const {
    ConnectionDB* conns = port->owner()->module()->conns();
    assert(conns != nullptr);

    std::set<const Port*> connected;
    if (port->isInput()) {
        auto source = conns->findSource(port->asInput());
        if (source != nullptr)
            connected.insert(source);
        auto driven = port->asInput()->findDriven();
        connected.insert(driven.begin(), driven.end());
    } else {
        std::set<InputPort*> sinks;
        conns->findSinks(port->asOutput(), sinks);
        connected.insert(sinks.begin(), sinks.end());
        auto deps = port->asOutput()->deps();
        connected.insert(deps.inputs.begin(), deps.inputs.end());
    }

    bool isConnected = false;
    for (auto c: connected) {
        if (_members.count(c) > 0)
            isConnected = true;
    }

    return isConnected;
}

void ScheduledRegion::addDrivers(const OutputPort* port) {
    auto dr = port->deps();
    if (dr.depType == DependenceRule::AND_FireOne) {
        for (auto inp: dr.inputs) {
            add(inp);
        }
    }
}

void ScheduledRegion::addDriven(const InputPort* port) {
    std::set<const Port*> fires;
    bool andFireOne = true;
    
    // Find output ports which are always driven by this port and only this
    // port or other member ports
    for (auto op: port->findDriven()) {
        auto dr = op->deps();
        if (dr.depType == DependenceRule::AND_FireOne){
            fires.insert(op);
            fires.insert(dr.inputs.begin(), dr.inputs.end());
        } else {
            andFireOne = false;
        }
    }

    if (andFireOne) {
        for (auto p: fires) {
            add(p);
        }
    }
}

bool ScheduledRegion::add(const Port* port,
                          const std::set<const Port*>& constPorts) {
    if (_members.count(port) > 0)
        // Already added! Cannot add it again, damnit!
        return false;

    if (!PortAllowed(port)) {
        return false;
    }

    Block* b = port->owner();
    ConnectionDB* conns = b->module()->conns();
    assert(conns != nullptr);
    
    if (!isConnectedToMe(port))
        return false;

    if (!BlockAllowedVirtual(b))
        return false;

    // Port is now a member. (Maybe virtual, maybe full. We'll determine that
    // when we finalize construction.)
    _members.insert(port);

#if 0
    printf("   Added %s (%p, %s) to %s\n",
           b->globalName().c_str(),
           b,
           cpp_demangle(typeid(*b).name()).c_str(),
           name().c_str());
#endif

    if (port->isOutput()) {
        addDrivers(port->asOutput());
    } else if (port->isInput()) {
        addDriven(port->asInput());
    } else {
        assert(false);
    }

    return true;
}

void ScheduledRegion::identifyIO() {
    if (_members.size() == 0)
        return;
    auto conns = (*_members.begin())->owner()->module()->conns();
    assert(conns != nullptr);

    // Identify all blocks which are full members.
    _fullMembers.clear();
    for (auto port: _members) {
        bool fullMem = true;
        auto b = port->owner();
        if (!BlockAllowedFull(b))
            continue;

        for (auto inp: b->inputs()) {
            if (_members.count(inp) == 0)
                fullMem = false;
        }
        for (auto outp: b->outputs()) {
            if (_members.count(outp) == 0)
                fullMem = false;
        }
        if (fullMem)
            _fullMembers.insert(b->getptr());
    }

    // Label as member, internal port, or external port
    _externalInputs.clear();
    _externalOutputs.clear();
    _internalInputs.clear();
    _internalOutputs.clear();
    set<Connection> vconns;
    for (auto port: _members) {
        if (port->isInput()) {
            auto ip = (InputPort*)port->asInput();
            auto source = conns->findSource(ip);
            if (source == nullptr ||
                _fullMembers.count(source->owner()) > 0) {
                // This is a full member connection
            } else {
                // We connect to the outside
                if (_members.count(source) > 0) {
                    // Source is a member, so virtual
                    _internalInputs.insert(ip);
                } else {
                    // Source is not a member, so external
                    _externalInputs.insert(ip);
                }
            }
        } else {
            auto op = (OutputPort*)port->asOutput();
            assert(op != nullptr);
            std::set<InputPort*> sinks;
            conns->findSinks(op, sinks);
            for (auto sink: sinks) {
                if (_fullMembers.count(sink->owner()) > 0) {
                    // This is a full member connection
                } else {
                    // We connect to the outside
                    if (_members.count(sink) > 0) {
                        // Source is a member, so virtual
                        _internalOutputs.insert(op);
                    } else {
                        // Source is not a member, so external
                        _externalOutputs.insert(op);
                    }
                }
            }
        }
    }
}

void ScheduledRegion::absorb() {
    // Make sure we have the final list of members
    identifyIO();

    _externalInputs.clear();
    _externalOutputs.clear();
    _internalInputs.clear();
    _internalOutputs.clear();

    if (_members.size() == 0)
        return;
    auto extConns = (*_members.begin())->owner()->module()->conns();
    assert(extConns != nullptr);

    // Use count pointers, just to make sure nothing gets collected during the
    // shuffle!
    set<BlockP> members;

    for (auto fm: _fullMembers) {
        members.insert(fm);
        fm->module(this);
    }

    // All member connections
    set<Connection> memberConnections;
    map<InputPort*, OutputPort*> virtualPorts;

    // Absorb based on all input port members. This should pull in _all_
    set<const Port*> membersCopy = _members.members;
    for (auto memb: membersCopy) {
        members.insert(memb->ownerP());
        if (memb->isInput()) {
            auto inp = (InputPort*)memb->asInput();
            auto source = extConns->findSource(inp);
            assert(source != nullptr);
            extConns->disconnect(source, inp);
            absorb(source, inp);
        }
    }

    // Absorb output channels. This should only get external outputs
    for (auto memb: membersCopy) {
        if (memb->isOutput()) {
            auto outp = (OutputPort*)memb->asOutput();
            set<InputPort*> sinks;
            extConns->findSinks(outp, sinks);
            for (auto sink: sinks) {
                // All internal connections should already be absorbed, so just
                // sorry about external connections
                if (!_members.contains(sink)) {
                    extConns->disconnect(outp, sink);
                    absorb(outp, sink);
                }
            }
        }

    }

    addNewMembers();
}

void ScheduledRegion::absorb(OutputPort* op, InputPort* ip) {
    auto extConns = module()->conns();
    assert(extConns != nullptr);

    auto opMemb = _members.contains(op);
    auto ipMemb = _members.contains(ip);
    assert(opMemb || ipMemb);

    if (_fullMembers.count(op->ownerP()) == 0) {
        // Is this an internal connection or an external one?
        auto intConn = opMemb;

        // Source is NOT a full member. Therefore it requires an input port.
        // Let's try to find one that already exists first. If we find one, put
        // it in 'internalOutput'
        OutputPort* internalOutput = nullptr;
        set<InputPort*> sinks;
        extConns->findSinks(op, sinks);
        for (auto sink: sinks) {
            if (( intConn && _internalInputs.count(sink) > 0) ||
                (!intConn && _externalInputs.count(sink) > 0)) {
                internalOutput = getDriver(sink);
            }
        }

        if (internalOutput == nullptr) {
            // We didn't find an existing internalOutput for this OP, so we
            // gotta create one.
            auto np = createInputPort(op->type());

            if (intConn) {
                unsigned num = _internalInputs.size();
                np->name(str(boost::format("int_input%1%") % num));
                _internalInputs.insert(np);
                _members.insert(np);
            } else {
                unsigned num = _externalInputs.size();
                np->name(str(boost::format("input%1%") % num));
                _externalInputs.insert(np);
            }
            internalOutput = getDriver(np);
            _members.insert(internalOutput);
            extConns->connect(op, np);
        }

        // Actual connection will be to this output
        op = internalOutput;
    }

    if (_fullMembers.count(ip->ownerP()) == 0) {
        // Is this an internal connection or an external one?
        auto intConn = ipMemb;
        // Sink is NOT a full member. Therefore it requires an output port.
        // Let's try to find one that already exists first. If we find one, put
        // it in 'internalInput'
        InputPort* internalInput = nullptr;
        auto source = extConns->findSource(ip);
        if (source != nullptr &&
            ( ( intConn && _internalOutputs.count(source) > 0) ||
              (!intConn && _externalOutputs.count(source) > 0) ) ) {

            internalInput = getSink(source);
        }

        if (internalInput == nullptr) {
            // We didn't find an existing internalOutput for this OP, so we
            // gotta create one.
            auto np = createOutputPort(ip->type());
            if (intConn) {
                unsigned num = _internalOutputs.size();
                np->name(str(boost::format("int_output%1%") % num));
                _internalOutputs.insert(np);
                _members.insert(np);
            } else {
                unsigned num = _externalOutputs.size();
                np->name(str(boost::format("output%1%") % num));
                _externalOutputs.insert(np);
            }
            internalInput = getSink(np);
            _members.insert(internalInput);
            extConns->connect(np, ip);
        }

        // Actual connection will be to this output
        ip = internalInput;
    }

    conns()->connect(op, ip);
}

const std::set<const InputPort*>& ScheduledRegion::calculateExecutionOrder(
    const OutputPort* op,
    ConnectionDB* conns) {
    auto f = _executesNotLaterThan.find(op);
    if (f != _executesNotLaterThan.end())
        return f->second;

    set<const InputPort*> ret;
    set<InputPort*> sinks;
    conns->findSinks(op, sinks);
    for (auto sink: sinks) {
        if (_members.contains(sink)) {
            ret.insert(sink);

            for (auto driven: sink->findDriven()) {
                if (_members.contains(driven)) {
                    auto recSink = calculateExecutionOrder(driven, conns);
                    ret.insert(recSink.begin(), recSink.end());
                }
            }
        }
    }

    if (_members.contains(op)) {
        _executesNotLaterThan[op] = ret;
        return _executesNotLaterThan[op];
    } else {
        static std::set<const InputPort*> EmptySet;
        return EmptySet;
    }
}

void ScheduledRegion::calculateOrder(ConnectionDB* conns) {
    // To get list of external/internal I/O
    identifyIO();

    for (auto extInp: _externalInputs) {
        auto source = conns->findSource(extInp);
        calculateExecutionOrder(source, conns);
    }

    for (auto fm: _fullMembers) {
        if (fm->inputs().size() == 0) {
            for (auto outp: fm->outputs()) {
                calculateExecutionOrder(outp, conns);
            }
        }
    }
}

void ScheduledRegion::debugPrint(string name) const {
    string fn = str(boost::format("%1%_%2%%3$03u.gv")
                        % module()->name()
                        % _name
                        % name);

    auto f =_design.workingDir()->create(fn);
    _design.gv()->writeModule(
        f,
        module(),
        true,
        "Scheduled region debug " + name);
    f->close();
}
void ScheduledRegion::checkOptFinalize() {
    // Check to make sure NED property is ensured
    set<const InputPort*> exti(_externalInputs.begin(), _externalInputs.end());
    for (auto exto: _externalOutputs) {
        // Note: This check won't work properly after Waits have been removed
        // but also has to be run after absorb() has been run.
        auto dr = findInternalDeps(exto);
        set<const InputPort*> deps(dr.inputs.begin(), dr.inputs.end());
        if (deps != exti) {
            debugPrint("NONED");
            printf("ERROR: NED property not held!\n");
            assert(false);
        }
    }

    // - Check to make sure acyclic
    assert(!hasCycle());

    // TODOs:
    // - Check to make sure virtual region is constant-time
    
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
                if (source != NULL) {
                    _conns.connect(source, sink);
                }
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

    // Run internal simplifications to get rid of things like dangling outputs,
    // which could mess up later stages.
    SimplifyPass sp(design());
    sp.runInternal(this);

    cleanInternal();
}

/**
 * Some internal data structures may contain references to ports which no
 * longer exist after optimization of internal blocks. Clean them out!
 */
void ScheduledRegion::cleanInternal() {
    // Find and remove deleted members
    set<BlockP> blocksToRemove;
    for (auto memb: _fullMembers) {
        bool foundConnection = false;
        for (auto inp: memb->inputs()) {
            if (conns()->findSource(inp) != nullptr)
                foundConnection = true;
        }
        for (auto outp: memb->outputs()) {
            if (conns()->countSinks(outp) > 0)
                foundConnection = true;
        }
        if (!foundConnection) {
            blocksToRemove.insert(memb);
        }
    }
    for (auto r: blocksToRemove) {
        _members.erase(r.get());
        _fullMembers.erase(r);
    }
    blocksToRemove.clear();

    // Find and remove deleted ports from members
    deque<const Port*> membToRemove;
    for (auto memb: _members) {
        if (memb->owner()->module() == this) {
            if (memb->isInput()) {
                if (conns()->findSource(memb->asInput()) == nullptr) {
                    membToRemove.push_back(memb); 
                }
            } else if (memb->isOutput()) {
                if (conns()->countSinks(memb->asOutput()) == 0) {
                    membToRemove.push_back(memb);
                }
            }
        }
    }
    for (auto p: membToRemove) {
        _members.erase(p);
    }

    // Find and remove deleted ports from ordering structures
    deque<const OutputPort*> toRemove;
    for (auto& opSetPair: _executesNotLaterThan) {
        if (!_members.contains(opSetPair.first)) {
            toRemove.push_back(opSetPair.first);
        } else {
            deque<const InputPort*> toRemoveInternal;
            for (auto ip: opSetPair.second) {
                if (!_members.contains(ip)) {
                    toRemoveInternal.push_back(ip);
                }
            }

            for (auto r: toRemoveInternal) {
                opSetPair.second.erase(r);
            }
        }
    }

    for (auto r: toRemove) {
        _executesNotLaterThan.erase(r);
    }

    _members.cleanInternal();
}

void ScheduledRegion::Members::cleanInternal() {
    /* Get rid of member pointers for deleted members */
    std::set<BlockP> blocks;
    for (auto memb: members) {
        blocks.insert(memb->ownerP());
    }
    memBlockPtrs.swap(blocks);
    blocks.clear();
}

bool ScheduledRegion::finalize(const OutputPort* root) {
    if (_members.size() == 0)
        return false;

    auto conns = root->owner()->module()->conns();
    assert(conns != nullptr);

    printf("Finalizing ScheduledRegion %s\n", globalName().c_str());

    // As of now, all external outputs depend on all external inputs. Now we
    // must determine if this violates the NED property and fix it if so.
    _members.shrinkToConstraints(root, conns);
    if (_members.size() == 0)
        return false;

    // Determine wait-based ordering of operations to make sure it is respected
    // later when we schedule. It is much easier to do this before be actually
    // absorb the blocks since after absorbing them we'll have to trace
    // dataflow through the SR module boundaries.
    calculateOrder(conns);
    if (_fullMembers.size() == 0)
        // Require at least one full member, for now.
        return false;
    
    // Find dangling outputs and add NullSinks
    for (auto memb: _members) {
        if (memb->isOutput()) {
            auto outp = memb->asOutput();
            std::set<InputPort*> sinks;
            conns->findSinks(outp, sinks);
            unsigned memSinkCount = 0;
            for (auto sink: sinks)
                if (_members.contains(sink))
                    memSinkCount += 1;
            if (memSinkCount == 0) {
                auto ns = new NullSink(outp->type());
                conns->connect((OutputPort*)outp, ns->din());
                _members.insert(ns->din());
            }
        }
    }

    // OK, time to actually bring everything inside
    absorb();

    // Now that we're a proper module, use our internal conns
    conns = this->conns();

    // Check sanity and remove things like waits and forks
    checkOptFinalize();

    // Find unused (dangling) input drivers and add NullSink
    for (auto inp: inputs()) {
        auto driver = getDriver(inp);
        if (conns->countSinks(driver) == 0) {
            auto ns = new NullSink(driver->type());
            conns->connect(driver, ns->din());
            _members.insert(ns->din());
        }
    }

    // Do a minimum clock latency scheduling
    scheduleMinimumClocks();

    return true;
}

void ScheduledRegion::addNewMembers() {
    set<Block*> blocks;
    conns()->findAllBlocks(blocks);
    // All contained blocks are full members
    for (auto b: blocks) {
        _fullMembers.insert(b->getptr());
        for (auto inp: b->inputs()) {
            _members.insert(inp);
        }

        for (auto outp: b->outputs()) {
            _members.insert(outp);
        }

        assert(BlockAllowedFull(b) || b->is<DummyBlock>());
    }

    // Go through all ports and add actual dataflow dep
    for (auto memb: _members) {
        if (memb->isInput()) {
            auto inp = memb->asInput();
            auto source = findInternalSource(inp);
            if (source != nullptr)
                _executesNotLaterThan[source].insert(inp);
        }
    }
}

void ScheduledRegion::scheduleMinimumClocks() {
    /*
     * Do a lazy scheduling of ports, working from the outputs on up. CycleNum
     * in this code refers to INVERSE TIME. So cycleNum == 0 means the last
     * cycle in this SR, and cycleNum == 1 means the second to last, ect.
     */

    _cycles.clear();
    _cycleIdx.clear();

    // If blocks have been added since ordering/memberizing, add them to the
    // order and member data.
    addNewMembers();

    set<const OutputPort*> inputSources;
    for (auto extInp: _externalInputs) {
        inputSources.insert(getDriver(extInp));
    }

    // Find deps by inverting _executesNotLaterThan
    map<const InputPort*, set<const OutputPort*> > remainingDeps;
    for (const auto& executesPair: _executesNotLaterThan) {
        for (auto ip: executesPair.second) {
            remainingDeps[ip].insert(executesPair.first);
        }
    }

    map<unsigned, deque<const InputPort*>> firingAtCycle;
    // Start with all outputs
    for (auto extOutp: _externalOutputs) {
        firingAtCycle[0].push_back(getSink(extOutp));
    }
    // And any input which leads nowhere
    for (auto memb: _members) {
        if (memb->isInput()) {
            auto inp = memb->asInput();
            if (inp->findDriven().size() == 0)
                firingAtCycle[0].push_back(inp);
        }
    }

    for (auto memb: _members) {
        if (memb->isOutput()) {
            auto outMemb = memb->asOutput();
            set<const InputPort*> sinks;
            findInternalSinks(outMemb, sinks);
            if (sinks.size() == 0) {
                debugPrint("DanglingOutputs");
                assert(sinks.size() > 0 && "Cannot have dangling outputs!");
            }
        }
    }

    // Iterate until all inputs have been fired
    do {
        unsigned cycleNum = _cycles.size();
        _cycles.insert(_cycles.begin(), Cycle(this));
        Cycle& cycle = _cycles.front();

        if (cycleNum > 100) {
            printf("ERROR: Giving up scheduling!\n");
            break;
        }

        // Iterate over everything firing this cycle
        //   Note: do not cache pointers into this data structure as it can be
        //   updated in this loop, invalidating those pointers.
        while (firingAtCycle[cycleNum].size() > 0) {
            auto firingIP = firingAtCycle[cycleNum].front();
            firingAtCycle[cycleNum].pop_front();
            cycle._firing.insert(firingIP);

            // These outputs MUST already have been available, even if they
            // aren't actually used.
            auto neededDeps = remainingDeps[firingIP];
            remainingDeps.erase(firingIP);

            auto dataSource = findInternalSource(firingIP);
            assert(dataSource != nullptr);
            assert(neededDeps.count(dataSource) > 0 ||
                   dataSource->owner()->is<DummyBlock>() ); // Sanity check
            // The data on this output port must be available this cycle!
            cycle._available.insert(dataSource);

            // Go through all the deps and determine if we have already
            // calculated it or if this is the first usage.
            for (auto dep: neededDeps) {
                if (inputSources.count(dep) > 0)
                    continue; // No need to schedule inputs

                const auto& otherUsers = _executesNotLaterThan[dep];
                bool alreadyUsed = false;
                for (auto otherUser: otherUsers) {
                    if (remainingDeps.count(otherUser) > 0)
                        alreadyUsed = true;
                }

                if (!alreadyUsed) {
                    // OK, this is the first use of this value. Schedule it to
                    // ensure it is available THIS clock cycle.
                    cycle._newValues.insert(dep);
                    auto dr = internalDeps(dep);
                    assert(dr.inputs.size() == dr.latencies.size());
                    for (unsigned i=0; i<dr.inputs.size(); i++) {
                        auto ip = dr.inputs[i];
                        auto lat = dr.latencies[i];
                        auto scheduledCycle =
                            cycleNum + lat.depth().registers();
                        firingAtCycle[scheduledCycle].push_back(ip);
                    }
                }
            }
        }
        firingAtCycle.erase(cycleNum);
    } while (remainingDeps.size() > 0);

    finalizeCycles();
}

void ScheduledRegion::finalizeCycles() {
    for (auto extInp: _externalInputs) {
        auto extSource = getDriver(extInp);
        _cycles[0]._newValues.insert(extSource);
        _cycleIdx[extSource] = 0;
    }

    for (unsigned i=0; i<_cycles.size(); i++) {
        Cycle& cycle = _cycles[i];
        for (auto nv: cycle._newValues) {
            _cycleIdx[nv] = i;
        }
        for (auto firing: cycle._firing) {
            _cycleIdx[firing] = i;
        }
    }
}

const OutputPort* ScheduledRegion::findInternalSource(
    const InputPort* sink) const {

    if (sink->owner()->is<DummyBlock>() &&
        sink->owner()->module() == this) {
        // This is one of our module inputs
        auto dbOut = sink->owner()->as<DummyBlock>()->dout();
        auto extInp = findExternalPortFromDriver(dbOut);
        if (_internalInputs.count(extInp)) {
            assert(false);
        }
    }

    // Find the correct conns to use
    if (sink->owner()->module() == nullptr)
        return nullptr;
    const ConnectionDB* conns = sink->owner()->module()->conns();
    assert(conns != nullptr);

    auto source = conns->findSource(sink);
    if (source == nullptr)
        return nullptr;
    else if (_members.contains(source)) 
        return source;
    else
        return nullptr;
}

void ScheduledRegion::findInternalSinks(
    const OutputPort* source,
    set<const InputPort*>& sinks) const {

    if (source->owner()->is<DummyBlock>() &&
        source->owner()->module() == this) {
        // This is one of our module inputs
        auto dbIn = source->owner()->as<DummyBlock>()->din();
        auto extOutp = findExternalPortFromSink(dbIn);
        if (_internalOutputs.count(extOutp)) {
            assert(false);
        }
    }

    // Find the correct conns to use
    if (source->owner()->module() == nullptr)
        return;
    const ConnectionDB* conns = source->owner()->module()->conns();
    assert(conns != nullptr);

    set<InputPort*> immedSinks;
    conns->findSinks(source, immedSinks);
    for (auto sink: immedSinks) {
        if (_members.contains(sink))
            sinks.insert(sink);
    }
}

std::set<OutputPort*> ScheduledRegion::findVirtualDeps(
    const InputPort* intIP,
    std::set<const InputPort*> seen) const {

    if (seen.count(intIP) > 0)
        return {};
    
    seen.insert(intIP);

    auto parent = module();
    assert(parent != nullptr);
    auto extconns = parent->conns();
    assert(extconns != nullptr);

    set<OutputPort*> ret;
    auto source = extconns->findSource(intIP);
    if (source != nullptr) {
        if (source->owner() == this)
            return {source};
        auto dr = source->deps();
        for (auto inp: dr.inputs) {
            auto vd = findVirtualDeps(inp, seen);
            ret.insert(vd.begin(), vd.end());
        }
    }
    return ret;
}

DependenceRule ScheduledRegion::internalDeps(const OutputPort* op) const {
    if (op->owner() == this) {
        auto sink = getSink(op);
        return DependenceRule(DependenceRule::AND_FireOne, {sink})
                    .combinational();
    } else if (op->owner()->module() == this &&
               op->owner()->is<DummyBlock>()) {
        auto ext = findExternalPortFromDriver(op);
        if (ext != nullptr) {
            return DependenceRule(DependenceRule::AND_FireOne, {ext})
                            .combinational();
        } 
    }
    return op->deps();
}

DependenceRule ScheduledRegion::findInternalDeps(
    const InputPort* ip,
    std::set<const InputPort*> seen) const {

    if (seen.count(ip) > 0)
        return {};

    if (ip->owner() == this &&
        _externalInputs.count((InputPort*)ip) > 0)
        return DependenceRule(DependenceRule::AND_FireOne, {ip});
    
    seen.insert(ip);

    auto source = findInternalSource(ip);
    if (source == nullptr) {
        return DependenceRule(DependenceRule::AND_FireOne, {});
    } else {
        DependenceRule ret(DependenceRule::AND_FireOne, {});
        for (auto dep: internalDeps(source).inputs) {
            auto newDR = findInternalDeps(dep, seen);
            ret.inputs.append(newDR.inputs.begin(), newDR.inputs.end());
        }
        return ret;
    }
}

DependenceRule ScheduledRegion::findInternalDeps(
    const OutputPort* extOP) const {
    return findInternalDeps(getSink(extOP));
}

bool ScheduledRegion::refine(ConnectionDB& conns) const {
    assert(false &&
           "SR Refinement not written. Needs to add Wait and Forks.");
    return false;
}

DependenceRule ScheduledRegion::deps(const OutputPort*) const {
    return DependenceRule(DependenceRule::AND_FireOne, inputs());
}

void ScheduledRegion::validityCheck() const {
    ContainerModule::validityCheck();
}

std::string ScheduledRegion::print() const {
    return str(
        boost::format("memb: %1%")
            % _members.size());
}

void ScheduledRegion::Members::erase(const Block* b) {
    for (auto inp: b->inputs()) {
        members.erase(inp);
    }
    for (auto outp: b->outputs()) {
        members.erase(outp);
    }
}

void ScheduledRegion::Members::shrinkToConstraints(
    const OutputPort* root, 
    ConnectionDB* conns) {

    struct DepInfo {
        const OutputPort* op;
        std::set<const Port*> allDeps;
        std::set<const OutputPort*> extDeps;
    };

    while (members.size() > 0) {
        removeIneligiblePorts();

        if (members.size() == 0)
            break;

        // Do we satisfy NED?
        auto extIns  = findExtIns(conns);
        auto extOuts = findExtOuts(conns);
        if (extOuts.size() == 0)
            // If we have no external outputs, then NED is guaranteed!
            return;

        vector<DepInfo> depInfo;
        const DepInfo* rootDI = nullptr;
        for (auto op: extOuts) {
            depInfo.push_back(DepInfo());
            DepInfo& di = depInfo.back();
            di.op = op;
            findDeps(op, conns, di.allDeps, di.extDeps);
        }

        for (auto& d: depInfo) {
            if (d.op == root)
                rootDI = &d;
        }

        if (rootDI == nullptr) {
            // The root is no longer an external output. Re-choose any root
            // that contains the root as a dep
            for (auto& d: depInfo) {
                if (d.allDeps.count(root) > 0)
                    rootDI = &d;
            }
        }

        if (rootDI == nullptr) {
            // The root is not part of any remaining output chain. Select any
            // output as new root.
            rootDI = &depInfo[0];
        }

        assert(rootDI != nullptr);

        unsigned highestDiff = 0;
        unsigned highestDiffIdx = 0;
        for (auto i=0u; i<depInfo.size(); i++) {
            const auto& jd = depInfo[i];
            vector<const OutputPort*> extDepDiff;
            std::set_symmetric_difference(
                rootDI->extDeps.begin(), rootDI->extDeps.end(),
                jd.extDeps.begin(), jd.extDeps.end(),
                std::back_inserter(extDepDiff));

            if (extDepDiff.size() > highestDiff) {
                highestDiff = extDepDiff.size();
                highestDiffIdx = i;
            }
            
#if 0
            printf("    %s(%p):%s -- %s(%p):%s: %lu (%lu vs. %lu)\n",
                   rootDI->op->owner()->globalName().c_str(),
                   rootDI->op->owner(),
                   rootDI->op->name().c_str(),
                   jd.op->owner()->globalName().c_str(),
                   jd.op->owner(),
                   jd.op->name().c_str(),
                   extDepDiff.size(),
                   rootDI->allDeps.size(),
                   jd.allDeps.size());
            printf("        ");
            for (auto extop: jd.extDeps) {
                printf("%s(%p):%s, ",
                       extop->owner()->globalName().c_str(),
                       extop->owner(),
                       extop->name().c_str());
            }
            printf("\n");
#endif
        }
    
        
        auto highestDiffDI = depInfo[highestDiffIdx];
        if (highestDiff == 0 &&
            highestDiffDI.extDeps == extIns) {
            // We DO satisfy NED!
            return; // So we're done!
        }

        // We don't satisfy NED!
        // Start trimming. Keep looping, checking, and trimming until
        // we satisfy NED!

        auto toRemove = highestDiffDI;
        // printf("Removing %p(%p), ", toRemove.op, toRemove.op->owner());
        members.erase(toRemove.op);
        auto dr = toRemove.op->deps();
        for (auto inp: dr.inputs) {
            members.erase(inp);
            // printf("%p, ", inp);
        }
        // printf("\n");
    }
}

void ScheduledRegion::Members::removeIneligiblePorts() {
    set<const Port*> toRemove;
    do {
        toRemove.clear();
        for (auto memb: members) {
            if (memb->isInput()) {
                for (auto driven: memb->asInput()->findDriven()) {
                    if (members.count(driven) == 0)
                        toRemove.insert(memb);
                }
            } else if (memb->isOutput()) {
                for (auto dep: memb->asOutput()->deps().inputs) {
                    if (members.count(dep) == 0)
                        toRemove.insert(memb);
                }
            } else {
                assert(false);
            }
        }
        for (auto r: toRemove) {
            members.erase(r);
        }
    } while (toRemove.size() > 0);
}

void ScheduledRegion::Members::findDeps(
    const OutputPort* op,
    ConnectionDB* conns,
    std::set<const Port*>& allDeps,
    std::set<const OutputPort*>& extDeps) const {

    // Make sure 'op' is a member!
    assert(members.count(op) > 0);

    // Ensure we don't recurse forever in a cycle
    if (allDeps.count(op) > 0)
        return;
    allDeps.insert(op);

    auto dr = op->deps();
    for (auto ip: dr.inputs) {
        if (members.count(ip) == 0)
            continue;

        allDeps.insert(ip);
        auto source = conns->findSource(ip);
        if (source != nullptr) {
            if (members.count(source) > 0) {
                findDeps(source, conns, allDeps, extDeps);
            } else {
                extDeps.insert(source);
            }
        }
    }
}

std::set<const OutputPort*> ScheduledRegion::Members::findExtIns(
    ConnectionDB* conns) const {
    set<const OutputPort*> ret;
    for (auto member: members) {
        if (member->isInput()) {
            auto inp = member->asInput();
            auto source = conns->findSource(inp);
            if (members.count(source) == 0) {
                ret.insert(source);
            }
        }
    }
    return ret;
}

std::set<const OutputPort*> ScheduledRegion::Members::findExtOuts(
    ConnectionDB* conns) const {
    set<const OutputPort*> ret;
    for (auto member: members) {
        if (member->isOutput()) {
            auto outp = member->asOutput();
            set<InputPort*> sinks;
            conns->findSinks(outp, sinks);
            if (sinks.size() == 0) {
                ret.insert(outp);
            }
            for (auto sink: sinks) {
                if (members.count(sink) == 0) {
                    // We drive an external!
                    ret.insert(outp);
                }
            }
        }
    }
    return ret;
}


std::set<const InputPort*> ScheduledRegion::Members::getAllInputs() const {
    set<const InputPort*> ret;
    for (auto memb: members) {
        if (memb->isInput()) {
            ret.insert(memb->asInput());
        }
    }
    return ret;
}

unsigned ScheduledRegion::clocks() const {
    return _cycles.size() - 1;
}

unsigned ScheduledRegion::findClockNum(const Port* p) const {
    auto f = _cycleIdx.find(p);
    if (f != _cycleIdx.end())
        return f->second;
    if (p->owner()->is<DummyBlock>()) {
        auto db = p->owner()->as<DummyBlock>();
        if (p->isInput()) {
            p = db->dout();
        } else {
            p = db->din();
        }
    } else if (p->owner() == this) {
        // Port is one of our ports!
        if (p->isInput()) {
            auto inp = (InputPort*)p->asInput();
            if (_externalInputs.count(inp) > 0)
                p = getDriver(inp);
            else if (_internalInputs.count(inp) > 0)
                p = module()->conns()->findSource(inp);
        } else {
            auto op = (OutputPort*)p->asOutput();
            if (_externalOutputs.count(op) > 0)
                p = getSink(op);
            else if (_internalOutputs.count(op) > 0)
                p = conns()->findSource(getSink(op));
        }
    }

    // Retry with port on other end of DummyBlock
    //   (Some code does really stupid things, let's just accomodate this)
    f = _cycleIdx.find(p);
    if (f != _cycleIdx.end())
        return f->second;

    return -1;
}

} // namespace llpm
