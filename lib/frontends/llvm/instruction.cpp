#include "instruction.hpp"

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <boost/function.hpp>

#include <llpm/std_library.hpp>

namespace llpm {

llvm::Type* LLVMInstruction::getInput(llvm::Instruction* ins) {
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
        LLVMPureInstruction(ins)
    { }

    static FlowInstruction* Create(llvm::Instruction* ins) {
        return new FlowInstruction(ins);
    }
};


typedef boost::function<LLVMInstruction* (llvm::Instruction*)> InsConstructor;
std::unordered_map<unsigned, InsConstructor > Constructors = {
    {llvm::Instruction::PHI, WrapperInstruction<Identity>::Create},

    // Integer binary operators
    {llvm::Instruction::Add, IntWrapperInstruction<IntAddition>::Create},
    {llvm::Instruction::Sub, IntWrapperInstruction<IntSubtraction>::Create},
    {llvm::Instruction::Mul, IntWrapperInstruction<IntMultiply>::Create},
    {llvm::Instruction::UDiv, IntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::SDiv, IntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::URem, IntWrapperInstruction<IntRemainder>::Create},
    {llvm::Instruction::SRem, IntWrapperInstruction<IntRemainder>::Create},

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
