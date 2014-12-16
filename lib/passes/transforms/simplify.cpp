#include "simplify.hpp"

#include <util/transform.hpp>

#include <deque>

using namespace std;

namespace llpm {

void SimplifyPass::run(Module* m) {
    Transformer t(m);

    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> origBlocks;
    conns->findAllBlocks(origBlocks);
    printf("Simplifying %s with %lu blocks\n", m->name().c_str(), origBlocks.size());

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
            if (dinT->getStructNumElements() == 2) {
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
    printf("    Simplified %s to %lu blocks\n", m->name().c_str(), blocks.size());
    
    // Apply this pass iteratively until convergence
    if (origBlocks != blocks) {
        // Did this pass do something?
        this->run(m);
    }
}

};
