#include "synthesize_mem.hpp"

#include <llpm/module.hpp>
#include <libraries/core/mem_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <libraries/synthesis/memory.hpp>

#include <llvm/IR/Constants.h>

#include <deque>

using namespace std;

namespace llpm {

void SynthesizeMemoryPass::synthesizeReg(ConnectionDB* conns,
                                         Register* orig) {
    // Transform Registers to RTLRegs
    // TODO: something more intelligent?
    RTLReg* rr = new RTLReg(orig->type());
    rr->name(orig->name());
    conns->remap(orig->write(), rr->write());

    deque< pair<InputPort*, OutputPort*> > reqs;
    Interface* client = conns->findClient(orig->read());
    if (client) {
        InterfaceMultiplexer* im = 
            dynamic_cast<InterfaceMultiplexer*>(client->owner());
        if (im) {
            for (unsigned i=0; i<im->servers_size(); i++) {
                Interface* imServer = im->servers(i);
                conns->remap(imServer, rr->newRead());
            }
            conns->removeBlock(im);
        } else {
            conns->remap(orig->read(), rr->newRead());
        }
    }
    conns->removeBlock(orig);
}

void SynthesizeMemoryPass::synthesizeFiniteArray(ConnectionDB* conns,
                                                 FiniteArray* arr) {
    auto& context = arr->type()->getContext();

    // For now, synthesize all arrays as 2RW port block rams
    // TODO: This is surely silly. Make it technology dependent, at least
    BlockRAM* bram = new BlockRAM(arr->type(), arr->depth(), 2);

    // For now, map reads to one port and writes to another
    
    // Read port:
    {
        Constant* readConst = new Constant(llvm::ConstantInt::getFalse(context));
        Constant* nullConst = new Constant(
            llvm::Constant::getNullValue(arr->type()));
        Join* reqJoin = new Join(bram->ports(0)->din()->type());
        conns->connect(reqJoin->dout(), bram->ports(0)->din());
        conns->connect(readConst->dout(), reqJoin->din(0));
        conns->connect(nullConst->dout(), reqJoin->din(1));
        conns->remap(arr->read()->din(), reqJoin->din(2));
        conns->remap(arr->read()->dout(), bram->ports(0)->dout());
    }

    // Write port:
    {
        Constant* writeConst = new Constant(llvm::ConstantInt::getTrue(context));
        Split* reqSplit = new Split(arr->write()->din()->type());
        conns->remap(arr->write()->din(), reqSplit->din());
        Join*  reqJoin = new Join(bram->ports(1)->din()->type());
        conns->connect(writeConst->dout(), reqJoin->din(0));
        conns->connect(reqSplit->dout(0), reqJoin->din(1));
        conns->connect(reqSplit->dout(1), reqJoin->din(2));
        conns->connect(reqJoin->dout(), bram->ports(1)->din());

        auto voidConst = Constant::getVoid(arr->module()->design());
        auto writeWait = new Wait(voidConst->dout()->type());
        conns->connect(voidConst->dout(), writeWait->din());
        conns->connect(bram->ports(1)->dout(), 
                       writeWait->newControl(bram->ports(1)->dout()->type()));
        conns->remap(arr->write()->dout(), writeWait->dout());
    }
}

void SynthesizeMemoryPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (auto&& block: blocks) {
        Register* reg = dynamic_cast<Register*>(block);
        if (reg != NULL)
            synthesizeReg(conns, reg);

        FiniteArray* arr = dynamic_cast<FiniteArray*>(block);
        if (arr != NULL)
            synthesizeFiniteArray(conns, arr);
    }
}

} // namespace llpm
