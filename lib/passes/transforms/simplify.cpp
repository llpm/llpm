#include "simplify.hpp"

#include <util/transform.hpp>

namespace llpm {

void SimplifyPass::run(Module* m) {
    Transformer t(m);

    ConnectionDB* conns = m->conns();
    assert(conns != NULL);

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    printf("Simplifying %s with %lu blocks\n", m->name().c_str(), blocks.size());

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
                Split* s = dynamic_cast<Split*>(driver->owner());
                
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

            // TODO: if a re-order occurs, does it matter? Check consumers
            // to determine if operands are commutative

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

    blocks.clear();
    conns->findAllBlocks(blocks);
    printf("    Simplified %s to %lu blocks\n", m->name().c_str(), blocks.size());
}

};
