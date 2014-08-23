#include "instruction.hpp"

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <boost/function.hpp>
#include <llvm/IR/Instructions.h>

#include <llpm/std_library.hpp>

namespace llpm {

llvm::Type* LLVMInstruction::GetInput(llvm::Instruction* ins) {

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

llvm::Type* LLVMInstruction::GetOutput(llvm::Instruction* ins) {
    return ins->getType();
}

template<typename Inner>
class WrapperInstruction: public LLVMPureInstruction {
    static llvm::Type* GetInput(llvm::Instruction* ins);

public:
    WrapperInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins, GetInput(ins), ins->getType())
    { }

    static WrapperInstruction* Create(llvm::Instruction* ins) {
        return new WrapperInstruction(ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        ConnectionDB& conns) const;

    virtual unsigned getNumHWOperands() const {
        if (_ins->getOpcode() == llvm::Instruction::PHI)
            return 1;
        return _ins->getNumOperands();
    }

    virtual bool hwIgnoresOperand(unsigned idx) const {
        if (_ins->getOpcode() == llvm::Instruction::PHI)
            return idx > 0;
        return false;
    }
};

template<typename C>
llvm::Type* WrapperInstruction<C>::GetInput(llvm::Instruction* ins) {
    if (ins->getOpcode() == llvm::Instruction::PHI)
        return ins->getOperand(0)->getType();
    return LLVMInstruction::GetInput(ins);
}

template<>
bool WrapperInstruction<Identity>::refine(
    std::vector<Block*>& blocks,
    ConnectionDB& conns) const
{
    Identity* b;
    if (_ins->getOpcode() == llvm::Instruction::PHI)
        b = new Identity(_ins->getType());
    else
        b = new Identity(_ins->getOperand(0)->getType());

    blocks.push_back(b);
    conns.remap(input(), b->din());
    conns.remap(output(), b->dout());
    return true;
}

template<>
bool WrapperInstruction<Multiplexer>::refine(
    std::vector<Block*>& blocks,
    ConnectionDB& conns) const
{
    vector<llvm::Type*> inpTypes = {
        _ins->getOperand(0)->getType(),
        _ins->getOperand(1)->getType(),
        _ins->getOperand(2)->getType()
    };
    auto split = new Split(inpTypes);
    auto b = new Multiplexer(2, _ins->getOperand(1)->getType());
    blocks.push_back(b);
    conns.remap(input(), split->din());
    conns.connect(split->dout(0), b->sel());
    conns.connect(split->dout(1), b->din(0));
    conns.connect(split->dout(2), b->din(1));
    conns.remap(output(), b->dout());
    return true;
}

template<typename Inner>
class TruncatingIntWrapperInstruction: public LLVMPureInstruction {
    Inner* New() const;

public:
    TruncatingIntWrapperInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins)
    { }

    static TruncatingIntWrapperInstruction* Create(llvm::Instruction* ins) {
        return new TruncatingIntWrapperInstruction(ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        ConnectionDB& conns) const {
        Inner* b = New();
        blocks.push_back(b);
        conns.remap(input(), b->din());
        IntTruncate* trunc = new IntTruncate(b->dout()->type(), output()->type());
        blocks.push_back(trunc);
        conns.connect(b->dout(), trunc->din());
        conns.remap(output(), trunc->dout());
        return true;
    }
};

template<>
IntAddition* TruncatingIntWrapperInstruction<IntAddition>::New() const {
    std::vector<llvm::Type*> ops;
    for (unsigned i=0; i<_ins->getNumOperands(); i++) {
        ops.push_back(_ins->getOperand(i)->getType());
    }
    return new IntAddition(ops);
}

template<>
IntMultiply* TruncatingIntWrapperInstruction<IntMultiply>::New() const {
    std::vector<llvm::Type*> ops;
    for (unsigned i=0; i<_ins->getNumOperands(); i++) {
        ops.push_back(_ins->getOperand(i)->getType());
    }
    return new IntMultiply(ops);
}

template<>
IntSubtraction* TruncatingIntWrapperInstruction<IntSubtraction>::New() const {
    return new IntSubtraction(_ins->getOperand(0)->getType(),
                              _ins->getOperand(1)->getType());
}

template<typename G>
G* TruncatingIntWrapperInstruction<G>::New() const {
    switch (_ins->getOpcode()) {
    case llvm::Instruction::UDiv:
        return new G(_ins->getOperand(0)->getType(),
                     _ins->getOperand(1)->getType(),
                     false);
    case llvm::Instruction::SDiv:
        return new G(_ins->getOperand(0)->getType(),
                     _ins->getOperand(1)->getType(),
                     true);
    case llvm::Instruction::URem:
        return new G(_ins->getOperand(0)->getType(),
                     _ins->getOperand(1)->getType(),
                     false);
    case llvm::Instruction::SRem:
        return new G(_ins->getOperand(0)->getType(),
                     _ins->getOperand(1)->getType(),
                     true);
    default:
        throw InvalidArgument("Don't know how to create int instruction!");
    }
}


template<typename Inner>
class IntWrapperInstruction: public LLVMPureInstruction {
    Inner* New() const;

public:
    IntWrapperInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins)
    { }

    static IntWrapperInstruction* Create(llvm::Instruction* ins) {
        return new IntWrapperInstruction(ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        ConnectionDB& conns) const {
        Inner* b = New();
        blocks.push_back(b);
        conns.remap(input(), b->din());
        conns.remap(output(), b->dout());
        return true;
    }
};

template<>
Shift* IntWrapperInstruction<Shift>::New() const {
    switch(_ins->getOpcode()) {
    case llvm::Instruction::Shl:
        return new Shift(_ins->getOperand(0)->getType(),
                         _ins->getOperand(1)->getType(),
                         Shift::Left,
                         Shift::Logical);
    case llvm::Instruction::LShr:
        return new Shift(_ins->getOperand(0)->getType(),
                         _ins->getOperand(1)->getType(),
                         Shift::Right,
                         Shift::Logical);
    case llvm::Instruction::AShr:
        return new Shift(_ins->getOperand(0)->getType(),
                         _ins->getOperand(1)->getType(),
                         Shift::Right,
                         Shift::Arithmetic);
    default:
        throw InvalidArgument("Don't know how to convert to shift!");
    }
}

template<>
Bitwise* IntWrapperInstruction<Bitwise>::New() const {
    Bitwise::Op op;
    switch(_ins->getOpcode()) {
    case llvm::Instruction::And:
        op = Bitwise::AND;
        break;
    case llvm::Instruction::Or:
        op = Bitwise::OR;
        break;
    case llvm::Instruction::Xor:
        op = Bitwise::XOR;
        break;
    default:
        throw InvalidArgument("Don't know how to convert to bitwise!");
    }

    return new Bitwise(2, _ins->getOperand(0)->getType(), op);
}

template<>
IntCompare* IntWrapperInstruction<IntCompare>::New() const {
    llvm::ICmpInst* icmp = llvm::dyn_cast<llvm::ICmpInst>(_ins);
    IntCompare::Cmp cmp;
    bool isSigned;
    switch(icmp->getPredicate()) {
    case llvm::CmpInst::ICMP_EQ:
        cmp = IntCompare::EQ;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_NE:
        cmp = IntCompare::NEQ;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_UGT:
        cmp = IntCompare::GT;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_UGE:
        cmp = IntCompare::GTE;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_ULT:
        icmp->swapOperands();
        cmp = IntCompare::GT;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_ULE:
        icmp->swapOperands();
        cmp = IntCompare::GTE;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_SGT:
        cmp = IntCompare::GT;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SGE:
        cmp = IntCompare::GTE;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SLT:
        icmp->swapOperands();
        cmp = IntCompare::GT;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SLE:
        icmp->swapOperands();
        cmp = IntCompare::GTE;
        isSigned = true;
        break;
    default:
        throw InvalidArgument("Don't know how to convert to IntCompare!");
    }

    return new IntCompare(_ins->getOperand(0)->getType(),
                          _ins->getOperand(1)->getType(),
                          cmp,
                          isSigned);
}


class FlowInstruction: public LLVMPureInstruction {
public:
    FlowInstruction(llvm::Instruction* ins) :
        LLVMPureInstruction(ins, GetInput(ins), GetOutput(ins))
    { }

    static llvm::Type* GetInput(llvm::Instruction* ins) {
        switch (ins->getOpcode()) {
        case llvm::Instruction::Br:
            if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
                return llvm::Type::getVoidTy(ins->getContext());
            else
                return ins->getOperand(0)->getType();
        default:
            return LLVMInstruction::GetInput(ins);
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

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        ConnectionDB& conns) const
    {
        auto b = new Identity(_ins->getOperand(0)->getType());
        blocks.push_back(b);
        conns.remap(input(), b->din());
        conns.remap(output(), b->dout());
        return true;
    }
};


typedef boost::function<LLVMInstruction* (llvm::Instruction*)> InsConstructor;
std::unordered_map<unsigned, InsConstructor > Constructors = {
    {llvm::Instruction::PHI, WrapperInstruction<Identity>::Create},
    {llvm::Instruction::Select, WrapperInstruction<Multiplexer>::Create},

    // Integer binary operators
    {llvm::Instruction::Add,  TruncatingIntWrapperInstruction<IntAddition>::Create},
    {llvm::Instruction::Sub,  TruncatingIntWrapperInstruction<IntSubtraction>::Create},
    {llvm::Instruction::Mul,  TruncatingIntWrapperInstruction<IntMultiply>::Create},
    {llvm::Instruction::UDiv, TruncatingIntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::SDiv, TruncatingIntWrapperInstruction<IntDivide>::Create},
    {llvm::Instruction::URem, TruncatingIntWrapperInstruction<IntRemainder>::Create},
    {llvm::Instruction::SRem, TruncatingIntWrapperInstruction<IntRemainder>::Create},
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
    {llvm::Instruction::Switch, FlowInstruction::Create},
};

LLVMInstruction* LLVMInstruction::Create(llvm::Instruction* ins) {
    assert(ins != NULL);
    auto f = Constructors.find(ins->getOpcode());
    if (f == Constructors.end())
        throw InvalidArgument(string("Instruction type unsupported: ") + ins->getOpcodeName());
    return f->second(ins);
}

} // namespace llpm
