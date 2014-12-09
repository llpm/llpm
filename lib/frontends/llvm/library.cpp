#include "library.hpp"


#include <libraries/core/logic_intr.hpp>
#include <frontends/llvm/instruction.hpp>
#include <libraries/util/types.hpp>

#include <llvm/IR/Instruction.h>
#include <llvm/IR/BasicBlock.h>

#include <util/llvm_type.hpp>

namespace llpm {

class LLVMBasicBlockRefiner : public LLVMRefiner<LLVMBasicBlock> {
public:
    virtual bool refine(
            const Block* block,
            ConnectionDB& conns) const
    {
        const LLVMBasicBlock* lbb = dynamic_cast<const LLVMBasicBlock*>(block);
        if (lbb == NULL)
            return false;

        vector<InputPort*> inputs;

        llvm::BasicBlock* bb = lbb->basicBlock();
        map<llvm::Instruction*, LLVMInstruction*> blockMap;
        map<llvm::Value*, OutputPort*> valueMap;

        cout << "Refining BB " << bb->getName().str() << endl;
        cout << valuestr(bb) << endl;

        // Construct each block
        for(llvm::Instruction& ins: bb->getInstList()) {
            auto block = LLVMInstruction::Create(lbb, &ins);
            blockMap[&ins] = block;
            valueMap[&ins] = block->output();

            // Connect up memory ports, if any
            auto req = block->memReqPort();
            if (req)
                conns.remap(lbb->reqPort(&ins), req);
            auto resp = block->memRespPort();
            if (resp)
                conns.remap(lbb->respPort(&ins), resp);
        }

        // Construct any constants it produces
        for(llvm::Constant* c: lbb->constants()) {
            Constant* block = new Constant(c);
            if (c->hasName())
                block->name(c->getName());
            valueMap[c] = block->dout();
        }

        // Add an extractor for each input and add it to the value
        // map
        auto inputMap = lbb->inputMap();
        for(auto p: inputMap) {
            llvm::Value* v = p.first;
            unsigned inputNum = p.second;
            Extract* e = new Extract(lbb->input()->type(), {inputNum});
            if (v->hasName())
                e->name(v->getName().str() + "_extractor");
            inputs.push_back(e->din());
            valueMap[v] = e->dout();
        }

        // Connect the inputs to each block
        for(llvm::Instruction& ins: bb->getInstList()) {
            LLVMInstruction* li = blockMap[&ins];
            assert(li != NULL);

            vector<InputPort*> inputPorts;
            // Create a 'join' node for multiple inputs if necessary
            if (li->getNumHWOperands() > 1) {
                vector<llvm::Type*> inTypes;
                for (unsigned i=0; i<ins.getNumOperands(); i++) {
                    if (!li->hwIgnoresOperand(i))
                        inTypes.push_back(ins.getOperand(i)->getType());
                }
                Join* inJoin = new Join(inTypes);
                if (ins.hasName())
                    inJoin->name(ins.getName().str() + "_input_join");
                conns.connect(inJoin->dout(), li->input());
                unsigned hwNum = 0;
                for (unsigned i=0; i<ins.getNumOperands(); i++) {
                    if (!li->hwIgnoresOperand(i)) {
                        inputPorts.push_back(inJoin->din(hwNum));
                        hwNum += 1;
                    }
                }
            } else {
                inputPorts.push_back(li->input());
            }

            // For each input, find the correct output port and
            // connect it
            unsigned hwNum = 0;
            for (unsigned i=0; i<ins.getNumOperands(); i++) {
                if (li->hwIgnoresOperand(i))
                    continue;
                llvm::Value* operand = ins.getOperand(i);
                auto f = valueMap.find(operand);
                if (f != valueMap.end()) {
                    OutputPort* port = f->second;
                    conns.connect(port, inputPorts[hwNum]);
                } else {
                    // If it's not local it better be a
                    // constant!
                    Constant* c = new Constant(operand);
                    if (operand->hasName())
                        c->name(operand->getName());
                    conns.connect(c->dout(), inputPorts[hwNum]);
                }
                hwNum += 1;
            }
        }

        // Create and connect up a basic block output
        Join* output = new Join(lbb->output()->type());
        if (bb->hasName())
            output->name("bb_" + bb->getName().str() + "output_join");
        auto outputMap = lbb->outputMap();
        for(auto p: outputMap) {
            llvm::Value* v = p.first;
            unsigned i = p.second;
            auto f = valueMap.find(v);
            if (f == valueMap.end()) {
                printf("Could not find output in value map:\n");
                v->dump();
                if (lbb->passthroughs().count(v) > 0)
                    printf("It's a passthrough!\n");
                printf("While processing this block:\n");
                bb->dump();
                assert(f != valueMap.end());
            }
            conns.connect(f->second, output->din(i));
        }

        // Inform the DB about the external connection mapping
        conns.remap(lbb->input(), inputs);
        conns.remap(lbb->output(), output->dout());

        return true;
    }
};

class LLVMControlRefiner : public LLVMRefiner<LLVMControl>
{
public:
    virtual bool refine(
            const Block* block,
            ConnectionDB& conns) const
    {
        const LLVMControl* c = dynamic_cast<const LLVMControl*>(block);
        assert(c != NULL);
        llvm::BasicBlock* bb = c->basicBlock()->basicBlock();

        Select* inputSelect = new Select(c->predecessors_size(), c->bbInput()->type());
        for(unsigned i=0; i<c->predecessors_size(); i++) {
            conns.remap(c->predecessors(i), inputSelect->din(i));
        }
        conns.remap(c->bbInput(), inputSelect->dout());

        llvm::TerminatorInst* ti = bb->getTerminator();
        assert(ti != NULL);

        if (c->successors_size() == 1) {
            const auto& map = c->successorMaps(0);
            StructTwiddler* st = new StructTwiddler(c->bbOutput()->type(), map);
            conns.remap(c->bbOutput(), st->din());
            conns.remap(c->successors(0), st->dout());
        } else if (c->successors_size() > 1) {
            unsigned sselIdx = c->basicBlock()->mapOutput(ti);
            Extract* successorSel = new Extract(c->bbOutput()->type(), {sselIdx});
            Router* rtr = new Router(c->successors_size(), c->bbOutput()->type());
            Join*   rtrJ = new Join(rtr->din()->type());
            conns.connect(rtrJ->dout(), rtr->din());
            conns.remap(c->bbOutput(), {successorSel->din(), rtrJ->din(1)});
            conns.connect(successorSel->dout(), rtrJ->din(0));


            for(unsigned i=0; i<c->successors_size(); i++) {
                const auto& map = c->successorMaps(i);
                StructTwiddler* st = new StructTwiddler(c->bbOutput()->type(), map);
                conns.connect(rtr->dout(i), st->din());
                conns.remap(c->successors(i), st->dout());
            }
        }

        return true;
    }
};

std::vector<Refinery::Refiner*> LLVMBaseLibrary::BuildCollection() {
    return {
        new LLVMControlRefiner(),
        new LLVMBasicBlockRefiner(),
    };
}

} // namespace llpm

