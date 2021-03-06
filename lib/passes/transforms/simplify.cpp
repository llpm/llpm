#include "simplify.hpp"

#include <util/transform.hpp>
#include <util/llvm_type.hpp>
#include <libraries/core/logic_intr.hpp>
#include <analysis/graph_queries.hpp>
#include <analysis/graph.hpp>

#include <deque>

using namespace std;

namespace llpm {

void SimplifyPass::eliminateNoops(Module* m) {
    Transformer t(m);
    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> blocks;
    conns->findAllBlocks(blocks);

    // Eliminate identities and other no-ops
    for (Block* b: blocks) {
        // Identify does nothing by definition
        Identity* ib = dynamic_cast<Identity*>(b);
        if (ib)
            t.remove(ib);

        // Selects with one input do nothing!
        Select* sb = dynamic_cast<Select*>(b);
        if (sb && sb->din_size() == 1)
            t.remove(sb);

        if (sb && sb->din_size() == 0) {
            auto never = new Never(sb->dout()->type());
            t.conns()->remap(sb->dout(), never->dout());
        }

        // Routers with one output do nothing!
        Router* rb = dynamic_cast<Router*>(b);
        if (rb && rb->dout_size() == 1)
            t.remove(rb);

        // Extracts with no selection path are probably invalid anyway
        Extract* eb = dynamic_cast<Extract*>(b);
        if (eb && eb->path().size() == 0) {
            t.remove(eb);
        }

        // Find & replace single-input multiplexes
        Multiplexer* mb = dynamic_cast<Multiplexer*>(b);
        if (mb) {
            auto dinT = mb->din()->type();
            assert(dinT->isStructTy());
            if (numContainedTypes(dinT) == 2) {
                // Just a selector and data field
                Extract* e = new Extract(dinT, {1});
                t.replace(mb, e);
            }
        }

        // Find splits driven by joins and replace with wait
        Split* s = dynamic_cast<Split*>(b);
        if (s) {
            auto driver = t.conns()->findSource(s->din());
            if (driver) {
                auto join = driver->owner()->as<Join>();
                if (join) {
                    assert(s->dout_size() == join->din_size());
                    for (unsigned i=0; i<join->din_size(); i++) {
                        auto w = new Wait(join->din(i)->type());
                        auto joinDriver = t.conns()->findSource(join->din(i));
                        if (joinDriver) {
                            t.conns()->connect(joinDriver, w->din());
                            w->newControl(t.conns(), join->dout());
                            vector<InputPort*> sSinks;
                            t.conns()->findSinks(s->dout(i), sSinks);
                            for (auto sink: sSinks) {
                                t.conns()->disconnect(s->dout(i), sink);
                                t.conns()->connect(w->dout(), sink);
                            }
                        }
                    }
                }
            }
        }

        // Find extracts driven by joins and replace with wait
        Extract* e = dynamic_cast<Extract*>(b);
        if (e) {
            auto driver = t.conns()->findSource(e->din());
            auto join = driver->owner()->as<Join>();
            if (join) {
                auto idx = e->path().front();
                assert(idx < join->din_size());
                auto w = new Wait(join->din(idx)->type());
                auto joinDriver = t.conns()->findSource(join->din(idx));
                if (joinDriver) {
                    if (e->path().size() == 1) {
                        t.conns()->connect(joinDriver, w->din());
                    } else {
                        auto path = e->path();
                        path.erase(path.begin());
                        auto newE = new Extract(joinDriver->type(), path);
                        t.conns()->connect(joinDriver, newE->din());
                        t.conns()->connect(newE->dout(), w->din());
                    }
                    w->newControl(t.conns(), join->dout());
                    vector<InputPort*> sSinks;
                    t.conns()->findSinks(e->dout(), sSinks);
                    for (auto sink: sSinks) {
                        t.conns()->disconnect(e->dout(), sink);
                        t.conns()->connect(w->dout(), sink);
                    }
                }
            }
        }

        // Find & replace routers with a constant route!
        Router* rtr = dynamic_cast<Router*>(b);
        if (rtr) {
            auto selDriver = queries::FindSubfieldDriver(m, rtr->din(), {0});
            if (selDriver != nullptr) {
                auto constSel = queries::FindConstant(m, selDriver);
                if (constSel != nullptr) {
                    assert(constSel->getType()->isIntegerTy());
                    auto constSelValue = constSel->getUniqueInteger();
                    assert(constSelValue.getLimitedValue() < rtr->dout_size());
                    auto never = new Never(rtr->dout(0)->type());
                    auto valExtract = new Extract(rtr->din()->type(), {1});
                    conns->remap(rtr->din(), valExtract->din());
                    conns->remap(rtr->dout(constSelValue.getLimitedValue()),
                                 valExtract->dout());
                    for (unsigned i=0; i<rtr->dout_size(); i++) {
                        if (i != constSelValue) {
                            conns->remap(rtr->dout(i), never->dout());
                        }
                    }
                    t.trash(rtr);
                }
            }
        }

        // Eliminate blocks which drive nothing
        auto outputs = b->outputs();
        bool noSinks = true;
        for (auto op: outputs) {
            vector<InputPort*> sinks;
            conns->findSinks(op, sinks);
            if (sinks.size() > 0)
                noSinks = false;
        }

        if (noSinks && !b->is<NullSink>())
            t.trash(b);
    }
}

void SimplifyPass::simplifyNullSinks(Module* m) {
    Transformer t(m);
    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> blocks;
    conns->findAllBlocks(blocks);

    // Eliminate NullSinks on inputs with other connections
    for (Block* b: blocks) {
        if (b->is<NullSink>()) {
            auto ns = b->as<NullSink>();
            auto src = m->conns()->findSource(ns->din());
            vector<InputPort*> sinks;
            m->conns()->findSinks(src, sinks);
            assert(sinks.size() > 0);
            if (sinks.size() > 1)
                t.trash(ns);
        }
    }

    // Push NullSinks up as far as possible
    for (Block* b: blocks) {
        if (b->is<NullSink>()) {
            auto ns = b->as<NullSink>();
            auto srcP = m->conns()->findSource(ns->din());
            if (srcP == nullptr)
                // This means that this NS was already deleted.
                continue;
            auto src = srcP->owner();
            vector<InputPort*> sinks;
            for (auto op: src->outputs()) {
                m->conns()->findSinks(op, sinks);
            }

            bool foundNonNS = false;
            set<Block*> toTrash;
            for (auto ip: sinks) {
                if (!ip->owner()->is<NullSink>())
                    foundNonNS = true;
                else
                    toTrash.insert(ip->owner());
            }
            
            if (foundNonNS)
                continue;

            // src drives only NullSinks. It can be trashed and replaced with NullSinks
            for (auto ip: src->inputs()) {
                auto newns = new NullSink(ip->type());
                m->conns()->remap(ip, newns->din());
            }

            for (auto b: toTrash)
                t.trash(b);
            t.trash(src);
        }
    }
}

void SimplifyPass::simplifyExtracts(Module* m) {
    Transformer t(m);
    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> blocks;
    conns->findAllBlocks(blocks);

    // Find all field extracts
    map<OutputPort*, set<unsigned>> fieldsUsed;
    for (Block* b: blocks) {
        Extract* eb = dynamic_cast<Extract*>(b);
        if (eb) {
            assert(eb->path().size() > 0);
            OutputPort* op = conns->findSource(eb->din());
            fieldsUsed[op].insert(eb->path()[0]);
        }
    }

    // Replace some extracts with splits
    for (auto pr: fieldsUsed) {
        // Rule: convert to split if 2 or more fields are used
        if (pr.second.size() >= 2) {
            OutputPort* op = pr.first;
            Split* s = new Split(op->type());
            conns->connect(op, s->din());

            vector<InputPort*> opSinks;
            conns->findSinks(op, opSinks);
            for (auto sink: opSinks) {
                Extract* eb = dynamic_cast<Extract*>(sink->owner());
                if (eb) {
                    auto path = eb->path();
                    assert(path.size() > 0);
                    unsigned fieldNum = path.front();
                    path.erase(path.begin());
                    assert(fieldNum < s->dout_size());
                    conns->disconnect(op, sink);

                    OutputPort* newOp = s->dout(fieldNum);
                    if (path.size() > 0) {
                        Extract* newEB = new Extract(newOp->type(), path);
                        conns->connect(newOp, newEB->din());
                        newOp = newEB->dout();
                    }

                    vector<InputPort*> users;
                    conns->findSinks(eb->dout(), users);
                    for (auto user: users) {
                        conns->disconnect(eb->dout(), user);
                        conns->connect(newOp, user);
                    }
                }
            }
        }
    }
}

void SimplifyPass::simplifyConstants(Module* m) {
    Transformer t(m);
    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    std::set<const Port*> constPorts;
    std::set<Block*> constBlocks;
    queries::FindConstants(m, constPorts, constBlocks);

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (auto mop: m->outputs()) {
        blocks.insert(m->getSink(mop)->owner());
    }
    for (auto block: blocks) {
        if (constBlocks.count(block) > 0 &&
            block->isnot<DummyBlock>())
            continue;

        for (auto ip: block->inputs()) {
            if (constPorts.count(ip) > 0) {
                // This input is constant. If possible, replace it with a
                // single constant block.
                auto op = conns->findSource(ip);
                if (op == nullptr)
                    continue;
                auto newConst = Constant::getEquivalent(op);
                if (newConst != nullptr && newConst->dout() != op) {
                    conns->disconnect(op, ip);
                    conns->connect(newConst->dout(), ip);
                }
            }
        }
    }
}

void SimplifyPass::runInternal(Module* m) {
    Transformer t(m);

    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> origBlocks;
    conns->findAllBlocks(origBlocks);
    unsigned origNum = origBlocks.size();

    eliminateNoops(m);
    simplifyNullSinks(m);
    simplifyExtracts(m);
    simplifyConstants(m);
    // TODO: Find and eliminate redundant (duplicate) blocks

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    if (blocks.size() < origNum) 
        printf("    Simplified %s to from %u to %lu blocks\n",
               m->name().c_str(), origNum, blocks.size());
    
    // Apply this pass iteratively until convergence
    if (origBlocks != blocks) {
        // Did this pass do something?
        this->run(m); // If so, run it again!
    }
}

void CanonicalizeInputs::runInternal(Module* mod) {
    Transformer t(mod);
    if (!t.canMutate())
        return;

    set<Block*> blocks;
    t.conns()->findAllBlocks(blocks);
    for (auto b: blocks) {
        if (b->is<Extract>() ||
            b->is<Identity>() ||
            b->is<Split>() ||
            b->is<Cast>() ||
            b->is<Wait>()) {
            continue;
        }
        for (auto ip: b->inputs()) {
            canonicalizeInput(t, ip);
        }
    }
}

void CanonicalizeInputs::canonicalizeInput(
        Transformer& t, InputPort* target) {
    // Set of things upon this input must wait but is not actually
    // dependent
    set<OutputPort*> waits;
    bool cast = false;

    deque<unsigned> extractions;
    OutputPort* source = NULL;
    InputPort* ip = target;
    while (ip != NULL) {
        InputPort* oldip = ip;
        source = t.conns()->findSource(ip);
        if (source == NULL) {
            ip = NULL;
            break;
        }
        
        Block* b = source->owner();
        if (b->is<Extract>()) {
            auto e = b->as<Extract>();
            const auto& path = e->path();
            extractions.insert(extractions.begin(),
                               path.begin(), path.end());
            ip = e->din();
        } else if (b->is<Split>()) {
            auto s = b->as<Split>();
            unsigned idx;
            for (idx = 0; idx < s->dout_size(); idx++)
                if (s->dout(idx) == source)
                    break;
            extractions.push_front(idx);
            ip = s->din();
        } else if (b->is<Join>()) {
            auto j = b->as<Join>();
            if (extractions.empty()) {
                // We need the whole contents of the join, so keep it
                // and end the search
                ip = NULL;
            } else {
                unsigned idx = extractions.front();
                extractions.pop_front();
                assert(idx < j->din_size());
                ip = j->din(idx);
                waits.insert(j->dout());
            }
        } else if (b->is<Identity>()) {
            ip = b->as<Identity>()->din();
        } else if (b->is<Cast>()) {
            auto c = b->as<Cast>();
            if (c->din()->type()->isSingleValueType() &&
                c->dout()->type()->isSingleValueType()) {
                cast = true;
                ip = b->as<Cast>()->din();
            } else {
                // If cast deals in structs, error out!
                ip = nullptr;
            }
        } else if (b->is<Wait>()) {
            auto w = b->as<Wait>();
            ip = w->din();
            for (unsigned i=0; i<w->controls_size(); i++) {
                auto wsource = t.conns()->findSource(w->controls(i));
                if (wsource != NULL)
                    waits.insert(wsource);
            }
        } else {
            // Don't know how to deal with node. Terminate search
            ip = NULL;
        }

        // Check for infinite loop
        assert(oldip != ip);
    }

    OutputPort* currentSource = t.conns()->findSource(target);
    if (source && source != currentSource) {
        t.conns()->disconnect(currentSource, target);

        if (target->type()->isVoidTy()) {
            waits.insert(source);
            auto c = Constant::getVoid(_design);
            source = c->dout();
        } else {
            if (extractions.size() > 0) {
                auto e = new Extract(source->type(),
                                     vector<unsigned>(extractions.begin(),
                                                      extractions.end()));
                assert(e->dout()->type() != NULL);
                t.conns()->connect(source, e->din());
                source = e->dout();
            }

            if (cast) {
                auto c = new Cast(source->type(), target->type());
                t.conns()->connect(source, c->din());
                source = c->dout();
            }
        }

        if (waits.size() > 0) {
            auto w = new Wait(source->type());
            t.conns()->connect(source, w->din());
            source = w->dout();
            for (auto c: waits) {
                t.conns()->connect(w->newControl(c->type()), c);
            }
        }

        t.conns()->connect(source, target);
    }
}

void SimplifyWaits::runInternal(Module* mod) {
    Transformer t(mod);
    if (!t.canMutate())
        return;

    set<Block*> blocks;
    t.conns()->findAllBlocks(blocks);
    unsigned ctr = 0;
    for (auto b: blocks) {
        if (b->is<Wait>()) {
            collectControls(t, b->as<Wait>());
            ctr++;
            if (ctr % 100 == 0) {
                printf(".");
                fflush(stdout);
            }
        }
    }
    if (ctr >= 100)
        printf("\n");
}

struct SimplifyWaitsVisitor: public Visitor<VisitOutputPort> {
    unsigned pass = 1;
    std::set<const OutputPort*> allDominators; 
    std::set<const OutputPort*> initPoints;

    Terminate visit(const ConnectionDB*,
                    const VisitOutputPort& edge)
    {
        auto op = edge.endPort();
        if (pass == 1) {
            allDominators.insert(op);
        }

        auto rule = op->deps();
        if (rule.depType != DependenceRule::AND_FireOne ||
            op->owner()->is<Constant>())
            return TerminatePath;
        return Continue;
    }
    
    Terminate pathEnd(const ConnectionDB*,
                      const VisitOutputPort& edge)
    {
        auto op = edge.endPort();
        if (op->owner()->is<Constant>() ||
            allDominators.count(op) > 0)
            return Continue;
        if (pass == 2)
            initPoints.insert(edge.endPort());
        return Continue;
    }
};

void SimplifyWaits::collectControls(
        Transformer& t, Wait* wait)
{
    SimplifyWaitsVisitor visitor;
    GraphSearch<SimplifyWaitsVisitor, DFS> search(t.conns(), visitor);
    search.go(vector<InputPort*>({wait->din()}));
    visitor.pass = 2;
    vector<InputPort*> init;
    wait->controls(init);
    search.go(init);

    if (visitor.initPoints.empty()) {
        auto driver = t.conns()->findSource(wait->din());
        t.conns()->disconnect(driver, wait->din());
        t.conns()->remap(wait->dout(), driver);
    } else {
        Wait* newWait = new Wait(wait->din()->type());
        t.conns()->remap(wait->din(), newWait->din());
        t.conns()->remap(wait->dout(), newWait->dout());

        for (auto op: visitor.initPoints) {
            newWait->newControl(t.conns(), (OutputPort*)op);
        }
    }
}


};
