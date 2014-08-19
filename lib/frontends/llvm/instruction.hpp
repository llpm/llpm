#ifndef __LLPM_LLVM_INSTRUCTION_HPP__
#define __LLPM_LLVM_INSTRUCTION_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMInstruction: public virtual Block {
protected:
    static llvm::Type* getInput(llvm::Instruction*);
    static llvm::Type* getOutput(llvm::Instruction*);

    llvm::Instruction* _ins;

    LLVMInstruction(llvm::Instruction* ins) :
        _ins(ins)
    { }

public:
    DEF_GET_NP(ins);

    virtual InputPort* input() = 0;
    virtual OutputPort* output() = 0;

    static LLVMInstruction* Create(llvm::Instruction*);

    virtual unsigned getNumHWOperands() const {
        return _ins->getNumOperands();
    }

    virtual bool hwIgnoresOperand(unsigned) const {
        return false;
    }
};

class LLVMPureInstruction: public LLVMInstruction, public Function {
protected:
    LLVMPureInstruction(llvm::Instruction* ins) :
        LLVMInstruction(ins),
        Function(getInput(ins), getOutput(ins))
    { }

    LLVMPureInstruction(llvm::Instruction* ins,
                        llvm::Type* inputType,
                        llvm::Type* outputType) :
        LLVMInstruction(ins),
        Function(inputType, outputType)
    { }


public:
    virtual InputPort* input() {
        return &_din;
    }
    virtual OutputPort* output() {
        return &_dout;
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_INSTRUCTION_HPP__
