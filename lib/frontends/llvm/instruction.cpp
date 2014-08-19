#include "instruction.hpp"

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <boost/function.hpp>
#include <llvm/IR/Instructions.h>

#include <llpm/std_library.hpp>

namespace llpm {

llvm::Type* LLVMInstruction::getInput(llvm::Instruction* ins) {

    if (ins->getNumOperands() == 0)
        return llvm::Type::getVoidTy(ins->getContext());
    if (ins->getNumOperands() == 1) {
        for (unsigned i=0; i<ins->getNumOperands(); i++) {
            return ins->getOperand(i)->getType();
        }
    }

    vector<llvm::Type*> ty;
    for (unsigned i=0; i<ins->getNumOperands(); i++) {
        ty.push_back(ins->getOperand(i)->getType());
    }
    return llvm::StructType::get(ins->getContext(), ty);
}

llvm::Type* LLVMInstruction::getOutput(llvm::Instruction* ins) {
    return ins->getType();
}

template<typename Inner>
class WrapperInstruction: public LLVMPureInstruction {
public:
    WrapperInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins)
    { }

    static WrapperInstruction* Create(llvm::Instruction* ins) {
        return new WrapperInstruction(ins);
    }
};

template<typename Inner>
class IntWrapperInstruction: public LLVMPureInstruction {
public:
    IntWrapperInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins)
    { }

    static IntWrapperInstruction* Create(llvm::Instruction* ins) {
        return new IntWrapperInstruction(ins);
    }
};

class FlowInstruction: public LLVMPureInstruction {
public:
    FlowInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins, getInput(ins), getOutput(ins))
    { }

    static llvm::Type* getInput(llvm::Instruction* ins) {
        switch (ins->getOpcode()) {
        case llvm::Instruction::Br:
            if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
                return llvm::Type::getVoidTy(ins->getContext());
            else
                return ins->getOperand(0)->getType();
        default:
            return LLVMInstruction::getInput(ins);
        }
    }

    static FlowInstruction* Create(llvm::Instruction* ins) {
        return new FlowInstruction(ins);
    }

    virtual unsigned getNumHWOperands() const {
        switch (_ins->getOpcode()) {
        case llvm::Instruction::Br:
            if (llvm::dyn_cast<llvm::BranchInst>(_ins)->isUnconditional())
                return 0;
            else
                return 1;
        default:
            return _ins->getNumOperands();
        }
    }

    virtual bool hwIgnoresOperand(unsigned idx) const {
        switch (_ins->getOpcode()) {
        case llvm::Instruction::Br:
            if (llvm::dyn_cast<llvm::BranchInst>(_ins)->isUnconditional())
                return true;
            else
                return idx > 0;
        default:
            return false;
        }
    }
};


typedef boost::function<LLVMInstruction* (llvm::Instruction*)> InsConstructor;
std::unordered_map<unsigned, InsConstructor > Constructors = {
    {llvm::Instruction::PHI, WrapperInstruction<Identity>::Create},
    {llvm::Instruction::Select, WrapperInstruction<Multiplexer>::Create},

    // Integer binary operators
    {llvm::Instruction::Add, IntWrapperInstruction<IntAddition>::Create},
    {llvm::Instruction::Sub, IntWrapperInstruction<IntSubtraction>::Create},
    {llvm::Instruction::Mul, IntWrapperInstruction<IntMultiply>::Create},
    {llvm::Instruction::UDiv, IntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::SDiv, IntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::URem, IntWrapperInstruction<IntRemainder>::Create},
    {llvm::Instruction::SRem, IntWrapperInstruction<IntRemainder>::Create},
    {llvm::Instruction::ICmp, IntWrapperInstruction<IntCompare>::Create},

    // Logical operators (integer operands)
    {llvm::Instruction::Shl, IntWrapperInstruction<Shift>::Create}, // Shift left  (logical)
    {llvm::Instruction::LShr, IntWrapperInstruction<Shift>::Create}, // Shift right (logical)
    {llvm::Instruction::AShr, IntWrapperInstruction<Shift>::Create}, // Shift right (arithmetic)
    {llvm::Instruction::And, IntWrapperInstruction<Bitwise>::Create},
    {llvm::Instruction::Or, IntWrapperInstruction<Bitwise>::Create},
    {llvm::Instruction::Xor, IntWrapperInstruction<Bitwise>::Create},

    // Control flow operators
    {llvm::Instruction::Ret, WrapperInstruction<Identity>::Create},
    {llvm::Instruction::Br, FlowInstruction::Create},
};

LLVMInstruction* LLVMInstruction::Create(llvm::Instruction* ins) {
    assert(ins != NULL);
    auto f = Constructors.find(ins->getOpcode());
    if (f == Constructors.end())
        throw InvalidArgument(string("Instruction type unsupported: ") + ins->getOpcodeName());
    return f->second(ins);
}

} // namespace llpm
