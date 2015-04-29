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
        // printf("Ins: %s\n", this->name().c_str());
    }

    void createName();

public:
    DEF_GET_NP(ins);

    virtual InputPort* input() = 0;
    virtual OutputPort* output() = 0;
    virtual const InputPort* input() const = 0;
    virtual const OutputPort* output() const = 0;

    virtual const OutputPort* memReqPort() const {
        return NULL;
    }

    virtual const InputPort* memRespPort() const {
        return NULL;
    }

    virtual OutputPort* memReqPort() {
        return NULL;
    }

    virtual InputPort* memRespPort() {
        return NULL;
    }

    virtual const OutputPort* callReqPort() const {
        return NULL;
    }

    virtual const InputPort* callRespPort() const {
        return NULL;
    }

    virtual OutputPort* callReqPort() {
        return NULL;
    }

    virtual InputPort* callRespPort() {
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

class LLVMImpureInstruction: public LLVMInstruction {
protected:
    LLVMImpureInstruction(const LLVMBasicBlock* bb,
                          llvm::Instruction* ins,
                          InputPort* input) :
        LLVMInstruction(bb, ins)
    {
        if (input != NULL)
            _inputVec = {input};
    }

    std::vector<InputPort*> _inputVec;

public:
    virtual DependenceRule depRule(const OutputPort* op) const;
    virtual const std::vector<InputPort*>& deps(const OutputPort*) const;
};

class LLVMConstant : public LLVMImpureInstruction, public Constant {
public:
    LLVMConstant(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMImpureInstruction(bb, ins, NULL),
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

    virtual DependenceRule depRule(const OutputPort* op) const {
        return Constant::depRule(op);
    }

    virtual const std::vector<InputPort*>& deps(const OutputPort* op) const {
        return Constant::deps(op);
    }
};

// To represent an instruction with which the base library cannot deal
class LLVMMiscInstruction : public LLVMImpureInstruction {
    InputPort _din;
    OutputPort _dout;

public:
    LLVMMiscInstruction(const LLVMBasicBlock* bb,
                        llvm::Instruction* ins) :
        LLVMImpureInstruction(bb, ins, &_din),
        _din(this, GetInput(ins), "x"),
        _dout(this, GetOutput(ins), "a")
    { }

    DEF_GET(din);
    DEF_GET(dout);

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

    virtual std::string print() const;
};

class LLVMLoadInstruction : public LLVMImpureInstruction {
    InputPort _din;
    OutputPort _dout;
    OutputPort _readReq;
    InputPort  _readResp;

public:
    LLVMLoadInstruction(const LLVMBasicBlock* bb,
                         llvm::Instruction* ins);

    static bool isByval(llvm::Value* val);
    static bool isByvalLoad(llvm::Instruction* ins);

    DEF_GET(readReq);
    DEF_GET(readResp);

    virtual const OutputPort* memReqPort() const {
        return &_readReq;
    }

    virtual const InputPort* memRespPort() const {
        return &_readResp;
    }

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

    static LLVMInstruction* Create(
        const LLVMBasicBlock* bb, llvm::Instruction* ins);
};

class LLVMStoreInstruction : public LLVMImpureInstruction {
    InputPort _din;
    OutputPort _dout;
    OutputPort _writeReq;
    InputPort  _writeResp;

public:
    LLVMStoreInstruction(const LLVMBasicBlock* bb,
                         llvm::Instruction* ins);

    DEF_GET(writeReq);
    DEF_GET(writeResp);

    virtual const OutputPort* memReqPort() const {
        return &_writeReq;
    }

    virtual const InputPort* memRespPort() const {
        return &_writeResp;
    }

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

class LLVMCallInstruction : public LLVMImpureInstruction {
    InputPort _din;
    OutputPort _dout;
    OutputPort _call;
    InputPort  _ret;

public:
    LLVMCallInstruction(const LLVMBasicBlock* bb,
                         llvm::Instruction* ins);

    DEF_GET(call);
    DEF_GET(ret);

    virtual bool refinable() const {
        return true;
    }
    virtual bool refine(ConnectionDB& conns) const;

    virtual bool hasState() const {
        return false;
    }

    virtual const OutputPort* callReqPort() const {
        return &_call;
    }

    virtual const InputPort* callRespPort() const {
        return &_ret;
    }

    virtual OutputPort* callReqPort() {
        return &_call;
    }

    virtual InputPort* callRespPort() {
        return &_ret;
    }
    virtual InputPort* input() {
        return &_din;
    }
    virtual OutputPort* output() {
        return &_dout;
    }

    virtual const InputPort* input() const {
        return &_din;
    }
    virtual const OutputPort* output() const {
        return &_dout;
    }

    static LLVMInstruction* Create(
        const LLVMBasicBlock* bb, llvm::Instruction* ins);
};

} // namespace llpm

#endif // __LLPM_LLVM_INSTRUCTION_HPP__
