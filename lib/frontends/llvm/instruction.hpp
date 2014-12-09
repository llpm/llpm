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

    virtual OutputPort* memReqPort() {
        return NULL;
    }

    virtual InputPort* memRespPort() {
        return NULL;
    }

    virtual unsigned getNumHWOperands() const {
        return GetNumHWOperands(_ins);
    }

    virtual bool hwIgnoresOperand(unsigned i) const {
        return HWIgnoresOperand(_ins, i);
    }

    static LLVMInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction*);
    static std::string NameInstruction(llvm::Value*);
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

// To represent an instruction with which the base library cannot deal
class LLVMMiscInstruction : public LLVMInstruction {
    InputPort _din;
    OutputPort _dout;

public:
    LLVMMiscInstruction(const LLVMBasicBlock* bb,
                        llvm::Instruction* ins) :
        LLVMInstruction(bb, ins),
        _din(this, GetInput(ins), "x"),
        _dout(this, GetOutput(ins), "a")
    { }

    virtual bool refinable() const {
        return false;
    }

    virtual bool hasState() const {
        return false;
    }

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

class LLVMLoadInstruction : public LLVMInstruction {
    InputPort _din;
    OutputPort _dout;
    OutputPort _readReq;
    InputPort  _readResp;

public:
    LLVMLoadInstruction(const LLVMBasicBlock* bb,
                         llvm::Instruction* ins);

    DEF_GET(readReq);
    DEF_GET(readResp);

    virtual OutputPort* memReqPort() {
        return &_readReq;
    }

    virtual InputPort* memRespPort() {
        return &_readResp;
    }

    virtual bool refinable() const {
        return true;
    }
    virtual bool refine(ConnectionDB& conns) const;

    virtual bool hasState() const {
        return false;
    }

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

    static LLVMLoadInstruction* Create(
        const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        return new LLVMLoadInstruction(bb, ins);
    }
};

class LLVMStoreInstruction : public LLVMInstruction {
    InputPort _din;
    OutputPort _dout;
    OutputPort _writeReq;
    InputPort  _writeResp;

public:
    LLVMStoreInstruction(const LLVMBasicBlock* bb,
                         llvm::Instruction* ins);

    DEF_GET(writeReq);
    DEF_GET(writeResp);

    virtual OutputPort* memReqPort() {
        return &_writeReq;
    }

    virtual InputPort* memRespPort() {
        return &_writeResp;
    }

    virtual bool refinable() const {
        return true;
    }
    virtual bool refine(ConnectionDB& conns) const;

    virtual bool hasState() const {
        return false;
    }

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

    static LLVMStoreInstruction* Create(
        const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        return new LLVMStoreInstruction(bb, ins);
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_INSTRUCTION_HPP__
