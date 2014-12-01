#ifndef __LLPM_LLVM_INSTRUCTION_HPP__
#define __LLPM_LLVM_INSTRUCTION_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <frontends/llvm/objects.hpp>
#include <refinery/refinery.hpp>
#include <libraries/core/logic_intr.hpp>

namespace llpm {

class LLVMInstruction: public virtual Block {
public:
    static llvm::Type* GetInput(llvm::Instruction*);
    static llvm::Type* GetOutput(llvm::Instruction*);
    static unsigned GetNumHWOperands(llvm::Instruction*);
    static bool HWIgnoresOperand(llvm::Instruction*, unsigned);

protected:
    llvm::Instruction* _ins;
    const LLVMBasicBlock* _bb;

    LLVMInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        _ins(ins),
        _bb(bb)
    {
        this->createName();
        printf("Ins: %s\n", this->name().c_str());
    }

    void createName();

public:
    DEF_GET_NP(ins);

    virtual InputPort* input() = 0;
    virtual OutputPort* output() = 0;
    virtual const InputPort* input() const = 0;
    virtual const OutputPort* output() const = 0;

    static LLVMInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction*);

    virtual unsigned getNumHWOperands() const {
        return GetNumHWOperands(_ins);
    }

    virtual bool hwIgnoresOperand(unsigned i) const {
        return HWIgnoresOperand(_ins, i);
    }
};

class LLVMPureInstruction: public LLVMInstruction, public Function {
protected:
    LLVMPureInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMInstruction(bb, ins),
        Function(GetInput(ins), GetOutput(ins))
    { }

    LLVMPureInstruction(const LLVMBasicBlock* bb,
                        llvm::Instruction* ins,
                        llvm::Type* inputType,
                        llvm::Type* outputType) :
        LLVMInstruction(bb, ins),
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

class LLVMConstant : public LLVMInstruction, public Constant {
public:
    LLVMConstant(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMInstruction(bb, ins),
        Constant(GetOutput(ins))
    { }

    virtual InputPort* input() {
        return NULL;
    }
    virtual OutputPort* output(){
        return dout();
    }

    virtual const InputPort* input() const {
        return NULL;
    }
    virtual const OutputPort* output() const {
        return dout();
    }

    virtual bool hasState() const {
        return false;
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_INSTRUCTION_HPP__
