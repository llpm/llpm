#include "scheduled_region.hpp"

#include <libraries/synthesis/pipeline.hpp>
#include <passes/transforms/simplify.hpp>
#include <analysis/graph_queries.hpp>
#include <libraries/synthesis/fork.hpp>
#include <libraries/synthesis/pipeline.hpp>

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
                new ScheduledRegion(cm, opDriver->owner(), crName);
            sr->grow(constPorts);
            sr->finalize(opDriver);
            const auto ext = sr->inputs();
            inputsToSee.insert(inputsToSee.end(),
                               ext.begin(), ext.end());
            counter++;
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

bool ScheduledRegion::BlockAllowedFull(Block* b) {
    if (!b->outputsTied())
        return false;

    const auto inputs = b->inputs();
    for (auto output: b->outputs()) {
        auto dr = b->deps(output);
        if (dr.depType != DependenceRule::AND_FireOne)
            return false;
        if (!std::equal(dr.inputs.begin(), dr.inputs.end(),
                        inputs.begin(), inputs.end()))
            return false;
    }

    if (b->is<Latch>() || b->is<PipelineRegister>())
        // Don't allow Latches and PRegs for now
        return false;

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

    if (b->is<Latch>() || b->is<PipelineRegister>())
        // Don't allow Latches and PRegs for now since we're making all
        // scheduled regions sub-cycle to start out. Once they support
        // pipelining, we'll allow this.
        return false;

    return true;
}

ScheduledRegion::ScheduledRegion(MutableModule* parent,
                                 Block* seed,
                                 std::string name) :
    ContainerModule(parent->design(), name),
    _parent(parent) {

    assert(BlockAllowedFull(seed));

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

    std::set<Port*> connected;
    if (port->isInput()) {
        auto source = conns->findSource(port->asInput());
        if (source != nullptr)
            connected.insert(source);
    } else {
        std::set<InputPort*> sinks;
        conns->findSinks(port->asOutput(), sinks);
        connected.insert(sinks.begin(), sinks.end());
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
        _members.insert(dr.inputs.begin(), dr.inputs.end());
    }
}

void ScheduledRegion::addDriven(const InputPort* port) {
    Block* b = port->owner();
    std::set<const Port*> fires;
    bool andFireOne = true;
    
    // Find output ports which are always driven by this port and only this
    // port or other member ports
    for (auto op: b->outputs()) {
        auto dr = op->deps();
        for (auto dep: dr.inputs) {
            if (dep == port) {
                if (dr.depType == DependenceRule::AND_FireOne){
                    fires.insert(op);
                    fires.insert(dr.inputs.begin(), dr.inputs.end());
                    break;
                } else {
                    andFireOne = false;
                }
            }
        }
    }

    if (andFireOne) {
        _members.insert(fires.begin(), fires.end());
    }
}

bool ScheduledRegion::add(const Port* port,
                          const std::set<const Port*>& constPorts) {
    if (_members.count(port) > 0)
        // Already added! Cannot add it again, damnit!
        return false;

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

    printf("   Added %s (%p, %s) to %s\n",
           b->globalName().c_str(),
           b,
           cpp_demangle(typeid(*b).name()).c_str(),
           name().c_str());

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
            _fullMembers.insert(b);
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

    // Inside connections
    set<Connection> inside;
    // Use count pointers
    set<BlockP> members;

    for (auto memb: _fullMembers) {
        members.emplace(memb->getptr());
        memb->module(this);
    }

    map<OutputPort*, InputPort*> newPorts;

    for (auto memb: _fullMembers) {
        for (auto ip: memb->inputs()) {
            auto source = extConns->findSource(ip);
            if (source == nullptr)
                continue;
            if (_fullMembers.count(source->owner()) > 0) {
                // Full membership connection
                inside.emplace(source, ip);
                extConns->disconnect(source, ip);
            } else {
                extConns->disconnect(source, ip);
                auto newPort = newPorts[source];
                if (newPort == nullptr) {
                    newPort = addInputPort(ip);
                    extConns->connect(source, newPort);
                    newPorts[source] = newPort;
                } else {
                    inside.emplace(getDriver(newPort), ip);
                }

                if (_members.count(source) > 0) {
                    _internalInputs.insert(newPort);
                    assert(_externalInputs.count(newPort) == 0);
                } else {
                    _externalInputs.insert(newPort);
                    assert(_internalInputs.count(newPort) == 0);
                }
            }
        }

        for (auto op: memb->outputs()) {
            set<InputPort*> sinks;
            extConns->findSinks(op, sinks);
            vector<InputPort*> extSinks;
            for (auto sink: sinks) {
                if (_fullMembers.count(sink->owner()) > 0) {
                    inside.emplace(op, sink);
                    extConns->disconnect(op, sink);
                } else {
                    extConns->disconnect(op, sink);
                    extSinks.push_back(sink);
                }
            }
            if (extSinks.size() > 0) {
                auto newPort = addOutputPort(op);
                for (auto sink: extSinks) {
                    extConns->connect(newPort, sink);

                    if (_members.count(sink) > 0) {
                        _internalOutputs.insert(newPort);
                        assert(_externalOutputs.count(newPort) == 0);
                    } else {
                        _externalOutputs.insert(newPort);
                        assert(_internalOutputs.count(newPort) == 0);
                    }
                }
            }
        }
    }

    for (auto c: inside) {
        conns()->connect(c.source(), c.sink());
    }

    for (auto p: _internalInputs) {
        p->name("int_" + p->name());
    }
    for (auto p: _internalOutputs) {
        p->name("int_" + p->name());
    }
}

void ScheduledRegion::checkOptFinalize() {
    // Check to make sure NED property is ensured
    // TODO
    

    // Eliminate Waits and Forks
#if 0
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
#endif

    SimplifyPass sp(design());
    sp.runInternal(this);
}

void ScheduledRegion::finalize(Port* port) {
    if (_members.size() == 0)
        return;

    // Find external ports
    //identifyIO();

    // As of now, all external outputs depend on all external inputs. Now we
    // must determine if this violates the NED property and fix it if so.
    
    // TODO!
    
    // OK, time to actually bring everything inside
    absorb();

    // Check sanity and remove things like waits and forks
    checkOptFinalize();
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

} // namespace llpm
