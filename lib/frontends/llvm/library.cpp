#include "library.hpp"


#include <libraries/core/logic_intr.hpp>
#include <frontends/llvm/instruction.hpp>
#include <libraries/util/types.hpp>

#include <llvm/IR/Instruction.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>

#include <util/llvm_type.hpp>

using namespace std;

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

        Wait* controlWait = new Wait(lbb->output()->type());
        inputs.push_back(controlWait->newControl(lbb->input()->type()));
        conns.remap(lbb->output(), controlWait->dout());

        llvm::BasicBlock* bb = lbb->basicBlock();
        map<llvm::Instruction*, LLVMInstruction*> blockMap;
        map<llvm::Value*, OutputPort*> valueMap;

        // cout << "Refining BB " << bb->getName().str() << endl;
        // cout << valuestr(bb) << endl;

        // Construct each block
        for(llvm::Instruction& ins: bb->getInstList()) {
            auto block = LLVMInstruction::Create(lbb, &ins);
            blockMap[&ins] = block;
            valueMap[&ins] = block->output();

            // Connect up memory ports, if any
            auto req = block->memReqPort();
            auto iface = lbb->mem(&ins);
            if (req) {
                assert(iface != NULL);
                conns.remap(iface->dout(), req);
            }
            auto resp = block->memRespPort();
            if (resp) {
                assert(iface != NULL);
                conns.remap(iface->din(), resp);
            }
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
        auto inputMap = lbb->nonPhiInputMap();
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
                    inJoin->name("bb_" + ins.getName().str() + "_input_join");
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
            if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
                // Once again, PHI nodes are special
                auto fInputNum = lbb->phiInputMap().find(phi);
                assert(fInputNum != lbb->phiInputMap().end());
                auto inputNum = fInputNum->second;
                Extract* e = new Extract(lbb->input()->type(), {inputNum});
                if (ins.hasName())
                    e->name(ins.getName().str() + "_extractor");
                inputs.push_back(e->din());
                assert(inputPorts.size() == 1);
                conns.connect(e->dout(), inputPorts[0]);

                // Add alias for the input values for any which may be
                // passthoughs
                for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                    llvm::Value* predV = phi->getIncomingValue(i);
                    valueMap[predV] = li->output();
                }
            } else {
                // Every other node performs normally
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
        conns.connect(output->dout(), controlWait->din());

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
                StructTwiddler* st =
                    new StructTwiddler(c->bbOutput()->type(), map);

                unsigned succIdx;
                switch (ti->getOpcode()) {
                case llvm::Instruction::Ret:
                    assert(ti->getNumSuccessors() <= 1);
                    succIdx = 0;
                    break;
                case llvm::Instruction::Br:
                    if (ti->getNumSuccessors() == 2) {
                        succIdx = (i == 0) ? 1 : 0;
                    } else {
                        assert(ti->getNumSuccessors() == 1);
                        succIdx = 0;
                    }
                    break;
                case llvm::Instruction::Switch:
                    succIdx = i;
                    break;
                default:
                    assert(false && "Haven't built code for this "
                                    "terminator yet");
                }

                conns.connect(rtr->dout(succIdx), st->din());
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

