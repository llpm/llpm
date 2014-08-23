#include "library.hpp"

#include <llvm/IR/Instruction.h>
#include <llvm/IR/BasicBlock.h>
#include <llpm/logic_intr.hpp>
#include <frontends/llvm/instruction.hpp>

namespace llpm {

template<typename T>
class LLVMRefiner : public BlockRefiner {
public:
    virtual bool handles(Block* b) const {
        return dynamic_cast<LLVMBasicBlock*>(b) != NULL;
    }
};

class LLVMBasicBlockRefiner : public LLVMRefiner<LLVMBasicBlock> {
public:
    virtual bool refine(
            const Block* block,
            std::vector<Block*>& blocks,
            ConnectionDB& conns) const
    {
        const LLVMBasicBlock* lbb = dynamic_cast<const LLVMBasicBlock*>(block);
        if (lbb == NULL)
            return false;

        vector<InputPort*> inputs;

        llvm::BasicBlock* bb = lbb->basicBlock();
        map<llvm::Instruction*, LLVMInstruction*> blockMap;
        map<llvm::Value*, OutputPort*> valueMap;

        // Construct each block
        for(llvm::Instruction& ins: bb->getInstList()) {
            auto block = LLVMInstruction::Create(&ins);
            blockMap[&ins] = block;
            valueMap[&ins] = block->output();
            blocks.push_back(block);
        }

        // Add an extractor for each input and add it to the value
        // map
        auto inputMap = lbb->inputMap();
        for(auto p: inputMap) {
            llvm::Value* v = p.first;
            unsigned inputNum = p.second;
            Extract* e = new Extract(lbb->input()->type(), {inputNum});
            inputs.push_back(e->din());
            valueMap[v] = e->dout();
            blocks.push_back(e);
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
                blocks.push_back(inJoin);
                conns.connect(inJoin->dout(), li->input());
                unsigned hwNum =0;
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
                    blocks.push_back(c);
                    conns.connect(c->dout(), inputPorts[hwNum]);
                }
                hwNum += 1;
            }
        }

        // Create and connect up a basic block output
        Join* output = new Join(lbb->output()->type());
        blocks.push_back(output);
        auto outputMap = lbb->outputMap();
        for(auto p: outputMap) {
            llvm::Value* v = p.first;
            unsigned i = p.second;
            auto f = valueMap.find(v);
            assert(f != valueMap.end());
            conns.connect(f->second, output->din(i));
        }

        // Inform the DB about the external connection mapping
        conns.remap(lbb->input(), inputs);
        conns.remap(lbb->output(), output->dout());

        return true;
    }
};

std::vector<Refinery<Block>::Refiner*> LLVMBaseLibrary::BuildCollection() {
    return {
        new LLVMBasicBlockRefiner(),
    };
}

} // namespace llpm

