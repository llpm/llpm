#ifndef __LLPM_LLVM_INSTRUCTION_HPP__
#define __LLPM_LLVM_INSTRUCTION_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMInstruction: public virtual Block {
public:
    static llvm::Type* GetInput(llvm::Instruction*);
    static llvm::Type* GetOutput(llvm::Instruction*);
    static unsigned GetNumHWOperands(llvm::Instruction*);
    static bool HWIgnoresOperand(llvm::Instruction*, unsigned);

protected:
    llvm::Instruction* _ins;

    LLVMInstruction(llvm::Instruction* ins) :
        _ins(ins)
    { }

public:
    DEF_GET_NP(ins);

    virtual InputPort* input() = 0;
    virtual OutputPort* output() = 0;
    virtual const InputPort* input() const = 0;
    virtual const OutputPort* output() const = 0;

    static LLVMInstruction* Create(llvm::Instruction*);

    virtual unsigned getNumHWOperands() const {
        return GetNumHWOperands(_ins);
    }

    virtual bool hwIgnoresOperand(unsigned i) const {
        return HWIgnoresOperand(_ins, i);
    }
};

class LLVMPureInstruction: public LLVMInstruction, public Function {
protected:
    LLVMPureInstruction(llvm::Instruction* ins) :
        LLVMInstruction(ins),
        Function(GetInput(ins), GetOutput(ins))
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
    virtual OutputPort* output(){
        return &_dout;
    }

    virtual const InputPort* input() const {
        return &_din;
    }
    virtual const OutputPort* output() const {
        return &_dout;
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_INSTRUCTION_HPP__
