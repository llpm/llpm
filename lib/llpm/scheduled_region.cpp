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
            // Do not admit clocked elements (YET!)
            // TODO: Allow clocked elements in the future
            if (lat.depth().registers() > 0)
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

unsigned ScheduledRegion::stepNumber(const Port* p) {
    auto f = _executionOrder.find(p);
    if (f != _executionOrder.end())
        return f->second;

    if (_members.count(p) == 0 &&
        p->owner()->module() != this) {
        return 0;
    }

    unsigned step;
    if (p->owner() == this) {
        if (p->isInput()) {
            auto ip = (InputPort*)p->asInput();
            if (_externalInputs.count(ip) > 0) {
                step = 0;
            } else if (_internalInputs.count(ip) > 0) {
                auto source = module()->conns()->findSource(ip);
                step = stepNumber(source);
            } else {
                assert(false && "Cannot resolve port type!");
            }
        } else {
            assert(p->isOutput());
            auto intip = getSink(p->asOutput());
            step = stepNumber(intip);
        }
    } else if (p->owner()->is<DummyBlock>()) {
        if (p->isOutput()) {
            assert(p->owner()->module() == this);
            auto extip = findExternalPortFromDriver(p->asOutput());
            if (extip != nullptr) {
                step = stepNumber(extip);
            } else {
                step = stepNumber(p->owner()->as<DummyBlock>()->din());
            }
        } else {
            assert(p->isInput());
            auto source = conns()->findSource(p->asInput());
            if (source == nullptr)
                step = 0;
            else
                step = stepNumber(source);
        }
    } else {
        auto conns = p->owner()->module()->conns();
        if (p->isOutput()) {
            auto dr = p->asOutput()->deps();
            if (dr.depType == DependenceRule::AND_FireOne) {
                unsigned max = 0;
                for (auto dep: dr.inputs) {
                    auto s = stepNumber(dep);
                    if (s > max)
                        max = s;
                }
                step = max + 1;
            } else {
                // Technically, this SR is invalid.
                // TODO: Error or warn on this condition?
                step = 0;
            }
        } else {
            assert(p->isInput());
            auto source = conns->findSource(p->asInput());
            assert(source != nullptr);
            if (source == nullptr)
                step = 0;
            else
                step = stepNumber(source);
        }
    }

#if 0
    printf("%s (%p, %s): %u\n",
           p->name().c_str(), p,
           p->owner()->globalName().c_str(), step);
#endif
    _executionOrder[p] = step;
    return step;
}

void ScheduledRegion::calculateOrder() {
    for (auto memb: _members) {
        stepNumber(memb);
    }
}

void ScheduledRegion::checkOptFinalize() {
    // Check to make sure NED property is ensured
    set<const InputPort*> exti(_externalInputs.begin(), _externalInputs.end());
    for (auto exto: _externalOutputs) {
        // Note: This check won't work properly after Waits have been removed
        // but also has to be run after absorb() has been run.
        auto dr = findInternalDeps(exto);
        set<const InputPort*> deps(dr.inputs.begin(), dr.inputs.end());
        assert(deps == exti && "NED property not held!");
    }

    // TODOs:
    // - Check to make sure acyclic
    // - Check to make sure virtual region is constant-time
    

    // Determine wait-based ordering of operations to make sure it is respected
    // later when we schedule.
    calculateOrder();
    
    // Eliminate Waits and Forks
    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (auto b: blocks) {
        if (b->is<Wait>()) {
            auto w = b->as<Wait>();
            OutputPort* source = _conns.findSource(w->din());
            if (_executionOrder[source] < _executionOrder[w->dout()]) {
                // If necessary, update the source's step number to respect the
                // wait conditions.
                _executionOrder[source] = _executionOrder[w->dout()];
            }
            vector<InputPort*> sinks;
            _conns.findSinks(w->dout(), sinks);
            for (auto sink: sinks) {
                _conns.disconnect(w->dout(), sink);
                if (source != NULL) {
                    auto id = new Identity(source->type());
                    _conns.connect(source, id->din());
                    _conns.connect(id->dout(), sink);
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
}

void ScheduledRegion::finalize(const OutputPort* root) {
    if (_members.size() == 0)
        return;

    printf("Finalizing ScheduledRegion %s\n", globalName().c_str());

    // As of now, all external outputs depend on all external inputs. Now we
    // must determine if this violates the NED property and fix it if so.
    _members.shrinkToConstraints(root, root->owner()->module()->conns());
    
    // OK, time to actually bring everything inside
    absorb();

    // Check sanity and remove things like waits and forks
    checkOptFinalize();
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

DependenceRule ScheduledRegion::findInternalDeps(
    const InputPort* ip,
    std::set<const InputPort*> seen) const {

    if (seen.count(ip) > 0)
        return {};
    
    seen.insert(ip);

    auto source = conns()->findSource(ip);
    if (source == nullptr) {
        if (ip->owner()->is<DummyBlock>()) {
            auto extip = findExternalPortFromDriver(
                ip->owner()->as<DummyBlock>()->dout());
            assert(extip != nullptr);
            if (_externalInputs.count(extip) > 0) {
                return DependenceRule(DependenceRule::AND_FireOne, {extip});
            } else { 
                assert(_internalInputs.count(extip) > 0);

                DependenceRule ret(DependenceRule::AND_FireOne, {});
                auto vd = findVirtualDeps(extip);
                for (auto intoDep: vd) {
                    auto sink = getSink(intoDep);
                    seen.insert(sink);
                    auto newDR = findInternalDeps(sink, seen);
                    ret.inputs.append(newDR.inputs.begin(), newDR.inputs.end());
                }
                return ret;
            }
        } else {
            return DependenceRule(DependenceRule::AND_FireOne, {});
        }
    } else {
        DependenceRule ret(DependenceRule::AND_FireOne, {});
        for (auto dep: source->deps().inputs) {
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
        auto extOuts = findExtOuts(conns);
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

        assert(rootDI != nullptr);

        unsigned lowestIsect;
        unsigned lowestIsectIdx;
        for (auto i=0u; i<depInfo.size(); i++) {
            const auto& jd = depInfo[i];
            vector<const OutputPort*> extDepIntersection;
            std::set_intersection(
                rootDI->extDeps.begin(), rootDI->extDeps.end(),
                jd.extDeps.begin(), jd.extDeps.end(),
                std::back_inserter(extDepIntersection));

            if (i == 0 ||
                extDepIntersection.size() < lowestIsect) {
                lowestIsect = extDepIntersection.size();
                lowestIsectIdx = i;
            }
            
            printf("    %s(%p):%s -- %s(%p):%s: %lu (%lu vs. %lu)\n",
                   rootDI->op->owner()->globalName().c_str(),
                   rootDI->op->owner(),
                   rootDI->op->name().c_str(),
                   jd.op->owner()->globalName().c_str(),
                   jd.op->owner(),
                   jd.op->name().c_str(),
                   extDepIntersection.size(),
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
        }
    
        if (lowestIsect == rootDI->extDeps.size()) {
            // We DO satisfy NED!
            return; // So we're done!
        }

        // We don't satisfy NED!
        // Start trimming. Keep looping, checking, and trimming until
        // we satisfy NED!

        auto toRemove = depInfo[lowestIsectIdx];
        printf("Removing %p(%p), ", toRemove.op, toRemove.op->owner());
        members.erase(toRemove.op);
        auto dr = toRemove.op->deps();
        for (auto inp: dr.inputs) {
            members.erase(inp);
            printf("%p, ", inp);
        }
        printf("\n");
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

std::set<const OutputPort*> ScheduledRegion::Members::findExtOuts(
    ConnectionDB* conns) const {
    set<const OutputPort*> ret;
    for (auto member: members) {
        if (member->isOutput()) {
            auto outp = member->asOutput();
            set<InputPort*> sinks;
            conns->findSinks(outp, sinks);

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

} // namespace llpm
