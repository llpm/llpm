#include "synthesize_mem.hpp"

#include <llpm/module.hpp>
#include <libraries/core/mem_intr.hpp>
#include <libraries/synthesis/memory.hpp>

#include <deque>

using namespace std;

namespace llpm {

void SynthesizeMemoryPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;


    // Transform Registers to RTLRegs
    // TODO: something more intelligent?
    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (auto&& block: blocks) {
        Register* orig = dynamic_cast<Register*>(block);
        if (orig == NULL)
            continue;

        RTLReg* rr = new RTLReg(orig->type());
        conns->remap(orig->write()->din(), rr->write()->din());
        conns->remap(orig->write()->dout(), rr->write()->dout());

        deque< pair<InputPort*, OutputPort*> > reqs;
        Interface* client = conns->findClient(orig->read());
        if (client) {
            InterfaceMultiplexer* im = 
                dynamic_cast<InterfaceMultiplexer*>(client->owner());
            if (im) {
                for (unsigned i=0; i<im->servers_size(); i++) {
                    Interface* imServer = im->servers(i);
                    reqs.push_back(
                        make_pair(imServer->din(), imServer->dout()));
                }
                conns->disconnect(im->client(), orig->read());
            }
        }

        if (reqs.empty()) {
            // Reg is not driven by an mux
            reqs.push_back(
                make_pair(orig->read()->din(), orig->read()->dout()));
        }

        for (auto p: reqs) {
            OutputPort* requestor = conns->findSource(p.first);
            Wait* w = new Wait(rr->read()->type());
            conns->disconnect(p.first, requestor);
            conns->connect(rr->read(), w->din());
            conns->connect(w->newControl(requestor->type()), requestor);

            vector<InputPort*> respPorts;
            conns->findSinks(p.second, respPorts);

            for (auto respPort: respPorts) {
                conns->disconnect(p.second, respPort);
                conns->connect(w->dout(), respPort);
            }
        }
    }
}

} // namespace llpm
