#include "simplify.hpp"

#include <util/transform.hpp>
#include <util/llvm_type.hpp>
#include <libraries/core/logic_intr.hpp>
#include <analysis/graph_queries.hpp>
#include <analysis/graph.hpp>

#include <deque>

using namespace std;

namespace llpm {

void SimplifyPass::runInternal(Module* m) {
    Transformer t(m);

    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> origBlocks;
    conns->findAllBlocks(origBlocks);
    unsigned origNum = origBlocks.size();

    set<Block*> blocks = origBlocks;

    // Eliminate identities and other no-ops
    for (Block* b: blocks) {
        Identity* ib = dynamic_cast<Identity*>(b);
        if (ib)
            t.remove(ib);

        Select* sb = dynamic_cast<Select*>(b);
        if (sb && sb->din_size() == 1)
            t.remove(sb);

        Router* rb = dynamic_cast<Router*>(b);
        if (rb && rb->dout_size() == 1)
            t.remove(rb);

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
    }

    // Eliminate blocks which drive nothing
    for (Block* b: blocks) {
        auto outputs = b->outputs();
        bool noSinks = true;
        for (auto op: outputs) {
            vector<InputPort*> sinks;
            conns->findSinks(op, sinks);
            if (sinks.size() > 0)
                noSinks = false;
        }

        if (noSinks)
            t.trash(b);
    }


    blocks.clear();
    conns->findAllBlocks(blocks);
    // Find all field extracts
    map<OutputPort*, set<unsigned>> fieldsUsed;
    for (Block* b: blocks) {
        Extract* eb = dynamic_cast<Extract*>(b);
        if (eb) {
            assert(eb->path().size() > 0);
            OutputPort* op = conns->findSource(eb->din()); 
            Multiplexer* mb = dynamic_cast<Multiplexer*>(b);
        if (mb)
            t.remove(mb);
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


    blocks.clear();
    conns->findAllBlocks(blocks);
    // Find and eliminate no-op split-join pairs
    for (Block* b: blocks) {
        Join* jb = dynamic_cast<Join*>(b);
        if (jb != NULL) {
            bool match = true;
            vector<OutputPort*> drivers;
            Split* split = NULL;

            // Collect drivers for all join inputs
            for (unsigned i=0; i<jb->din_size(); i++) {
                auto input = jb->din(i);
                auto driver = conns->findSource(input);
                drivers.push_back(driver); 
                Split* s = (driver == NULL) ? NULL : dynamic_cast<Split*>(driver->owner());
                
                // Populate driver split if not already
                if (split == NULL)
                    split = s;

                // Is this driver a split and the _same_ split?
                if (s == NULL || s != split || s->dout_size() != jb->din_size()) {
                    match = false;
                    break;
                }
            }

            if (!match)
                continue;

            // Ensure that split-join does not re-order
            for (unsigned i=0; i<split->dout_size(); i++) {
                if (drivers[i] != split->dout(i))
                    match = false;
            }

            if (!match) {
                // Split-join pair re-orders fields. Does it matter? Check
                // consumers to determine if operands are commutative
                vector<InputPort*> sinks;
                conns->findSinks(jb->dout(), sinks);
                match = true;
                for (InputPort* ip: sinks) {
                    if (!ip->owner()->inputCommutative(ip)) {
                        match = false;
                    }
                }
            }

            if (match) {
                // Samely-ordered match? Total no-op, so remove the pair
                OutputPort* splitDriver = conns->findSource(split->din());
                vector<InputPort*> joinSinks;
                conns->findSinks(jb->dout(), joinSinks);

                conns->disconnect(splitDriver, split->din());
                for (InputPort* sink: joinSinks) {
                    conns->disconnect(jb->dout(), sink);
                    conns->connect(splitDriver, sink);
                }

                conns->removeBlock(jb);
                conns->removeBlock(split);
            }
        }
    }

    // TODO: Find and eliminate redundant (duplicate) blocks

    blocks.clear();
    conns->findAllBlocks(blocks);
    if (blocks.size() < origNum) 
        printf("    Simplified %s to from %u to %lu blocks\n",
               m->name().c_str(), origNum, blocks.size());
    
    // Apply this pass iteratively until convergence
    if (origBlocks != blocks) {
        // Did this pass do something?
        this->run(m);
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
            cast = true;
            ip = b->as<Cast>()->din();
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
    for (auto b: blocks) {
        if (b->is<Wait>()) {
            collectControls(t, b->as<Wait>());
        }
    }
}

struct SimplifyWaitsVisitor: public Visitor<IOEdge> {
    unsigned pass = 1;
    std::set<OutputPort*> allDominators; 
    std::set<OutputPort*> initPoints;

    Terminate visit(const ConnectionDB*,
                    const IOEdge& edge)
    {
        auto op = edge.endPort();
        if (pass == 1) {
            allDominators.insert(op);
        }

        auto rule = op->depRule();
        if (rule.inputType() != DependenceRule::AND ||
            rule.outputType() != DependenceRule::Always ||
            op->owner()->is<Constant>())
            return TerminatePath;
        return Continue;
    }
    
    Terminate pathEnd(const ConnectionDB*,
                      const IOEdge& edge)
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
    search.go(vector<InputPort*>({wait->controls()}));

    if (visitor.initPoints.empty()) {
        auto driver = t.conns()->findSource(wait->din());
        t.conns()->disconnect(driver, wait->din());
        t.conns()->remap(wait->dout(), driver);
    } else {
        Wait* newWait = new Wait(wait->din()->type());
        t.conns()->remap(wait->din(), newWait->din());
        t.conns()->remap(wait->dout(), newWait->dout());

        for (auto op: visitor.initPoints) {
            newWait->newControl(t.conns(), op);
        }
    }
}


};
