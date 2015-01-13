#include "instruction.hpp"

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Constants.h>

#include <libraries/core/std_library.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>

using namespace std;

namespace llpm {

void LLVMInstruction::createName() {
    this->name(NameInstruction(_ins));
}

std::string LLVMInstruction::NameInstruction(llvm::Value* val) {
    if (val->hasName()) {
        return val->getName();
    }

    llvm::Instruction* ins = llvm::dyn_cast_or_null<llvm::Instruction>(val);
    if (ins == NULL)
        return "";
    unsigned idx = 0;
    auto bb = ins->getParent();
    for (auto iter = bb->begin();
         iter != bb->end() && (llvm::Instruction*)iter != ins;
         iter++) {
        idx ++;
    }
    return str(boost::format("%1%i%2%")
                    % bb->getName().str()
                    % idx);
}

unsigned LLVMInstruction::GetNumHWOperands(llvm::Instruction* ins) {
    switch (ins->getOpcode()) {
    case llvm::Instruction::PHI:
        return 1;
    case llvm::Instruction::Br:
        if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
            return 0;
        else
            return 1;
    default:
        return ins->getNumOperands();
    }
}

bool LLVMInstruction::HWIgnoresOperand(llvm::Instruction* ins, unsigned idx) {
    switch (ins->getOpcode()) {
    case llvm::Instruction::PHI:
        return idx > 0;
    case llvm::Instruction::Br:
        if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
            return true;
        else
            return idx > 0;
    default:
        return false;
    }
}

llvm::Type* LLVMInstruction::GetInput(llvm::Instruction* ins) {
    /**
     * Special case inputs
     */
    switch(ins->getOpcode()) {
    case llvm::Instruction::Br:
        if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
            return llvm::Type::getVoidTy(ins->getContext());
        else
            return ins->getOperand(0)->getType();
    case llvm::Instruction::PHI:
        return ins->getOperand(0)->getType();
    }

    /**
     * Default inputs
     */
    if (GetNumHWOperands(ins) == 0)
        return llvm::Type::getVoidTy(ins->getContext());
    if (GetNumHWOperands(ins) == 1) {
        for (unsigned i=0; i<ins->getNumOperands(); i++) {
            if (!HWIgnoresOperand(ins, i))
                return LLVMFunction::sanitizeType(
                            ins->getOperand(i));
        }
    }

    vector<llvm::Type*> ty;
    for (unsigned i=0; i<ins->getNumOperands(); i++) {
        if (!HWIgnoresOperand(ins, i))
            ty.push_back(ins->getOperand(i)->getType());
    }
    return llvm::StructType::get(ins->getContext(), ty);
}

llvm::Type* LLVMInstruction::GetOutput(llvm::Instruction* ins) {
    switch (ins->getOpcode()) {
    case llvm::Instruction::Br:
        if (llvm::dyn_cast<llvm::BranchInst>(ins)->isUnconditional())
            return llvm::Type::getVoidTy(ins->getContext());
        else
            return ins->getOperand(0)->getType();
    case llvm::Instruction::Ret:
        if (ins->getNumOperands() > 0)
            return ins->getOperand(0)->getType();
        return llvm::Type::getVoidTy(ins->getContext());
    default:
        return ins->getType();
    }
}

DependenceRule LLVMImpureInstruction::depRule(
    const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    return DependenceRule(DependenceRule::AND, DependenceRule::Always);
}

const std::vector<InputPort*>& LLVMImpureInstruction::deps(
    const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    if (op == output())
        return inputs();
    else if (op == memReqPort())
        return _inputVec;
    assert(false && "Unknown port!");
}

template<typename Inner>
class WrapperInstruction: public LLVMPureInstruction {
public:
    WrapperInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMPureInstruction(bb, ins, GetInput(ins), GetOutput(ins))
    { }

    static WrapperInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        return new WrapperInstruction(bb, ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const;
};


template<>
bool WrapperInstruction<Identity>::refine(ConnectionDB& conns) const
{
    auto b = new Identity(LLVMInstruction::GetOutput(_ins));

    conns.remap(input(), b->din());
    conns.remap(output(), b->dout());

    if (_ins->getNumOperands() == 0) {
        // No input, need a constant
        Constant* c;
        if (_ins->getType()->isVoidTy())
            c = new Constant(_ins->getType());
        else
            c = new Constant(llvm::Constant::getNullValue(_ins->getType()));
        conns.connect(c->dout(), b->din());
    }
    return true;
}

template<>
bool WrapperInstruction<Multiplexer>::refine(
    ConnectionDB& conns) const
{
    vector<llvm::Type*> inpTypes = {
        _ins->getOperand(0)->getType(),
        _ins->getOperand(1)->getType(),
        _ins->getOperand(2)->getType()
    };
    auto split = new Split(inpTypes);
    auto m = new Multiplexer(2, _ins->getOperand(1)->getType());
    auto b = new Join( m->din()->type() );
    conns.connect(b->dout(), m->din());
    conns.remap(input(), split->din());
    conns.connect(split->dout(0), b->din(0));
    // Reverse the second and third inputs since select is 'true'
    // result then 'false' result.
    conns.connect(split->dout(1), b->din(2));
    conns.connect(split->dout(2), b->din(1));
    conns.remap(output(), m->dout());
    return true;
}

template<>
bool WrapperInstruction<IntExtend>::refine(
    ConnectionDB& conns) const
{
    assert(_ins->getNumOperands() == 1);
    llvm::CastInst* ci = llvm::dyn_cast<llvm::CastInst>(_ins);
    assert(ci != NULL);
    unsigned diff = ci->getDestTy()->getScalarSizeInBits()
                        - ci->getSrcTy()->getScalarSizeInBits();
    bool sign = ci->getOpcode() == llvm::Instruction::SExt;
    IntExtend* b = new IntExtend(diff, sign, ci->getSrcTy());

    conns.remap(input(), b->din());
    conns.remap(output(), b->dout());
    return true;
}

template<>
bool WrapperInstruction<Extract>::refine(
    ConnectionDB& conns) const
{
    llvm::ExtractElementInst* ee =
        llvm::dyn_cast_or_null<llvm::ExtractElementInst>(_ins);
    if (ee != NULL) {
        llvm::Value* indexOp = ee->getIndexOperand();
        llvm::ConstantInt* ci =
            llvm::dyn_cast_or_null<llvm::ConstantInt>(indexOp);

        if (ci != NULL) {
            auto s = new Split(input()->type());
            auto e = new Extract(ee->getVectorOperand()->getType(),
                                 {(unsigned)ci->getLimitedValue()});
            conns.connect(s->dout(0), e->din());
            conns.remap(input(), s->din());
            conns.remap(output(), e->dout());
            return true;
        }

        //TODO: dynamic index selection
    }
    return false;
}

template<>
bool WrapperInstruction<ReplaceElement>::refine(
    ConnectionDB& conns) const
{
    llvm::InsertElementInst* ie =
        llvm::dyn_cast_or_null<llvm::InsertElementInst>(_ins);
    if (ie != NULL) {
        llvm::Value* indexOp = ie->getOperand(2);
        llvm::ConstantInt* ci =
            llvm::dyn_cast_or_null<llvm::ConstantInt>(indexOp);

        if (ci != NULL) {
            auto s = new Split(input()->type());
            conns.remap(input(), s->din());
            auto j = new Join({s->dout(0)->type(), s->dout(1)->type()});
            conns.connect(s->dout(0), j->din(0));
            conns.connect(s->dout(1), j->din(1));
            auto re = new ReplaceElement(ie->getOperand(0)->getType(),
                                         ci->getLimitedValue());
            conns.connect(j->dout(), re->din());
            conns.remap(output(), re->dout());
            return true;
        } else {
            auto re = new ReplaceElement(ie->getOperand(0)->getType());

            auto s = new Split(input()->type());
            conns.remap(input(), s->din());
            auto j = new Join(re->din()->type());
            conns.connect(j->dout(), re->din());
            conns.connect(s->dout(0), j->din(0));
            conns.connect(s->dout(1), j->din(1));

            auto trunc = new IntTruncate(s->dout(2)->type(),
                                         j->din(2)->type());
            conns.connect(s->dout(2), trunc->din());
            conns.connect(trunc->dout(), j->din(2));

            conns.remap(input(), s->din());
            conns.remap(output(), re->dout());
            return true;
        }
    }
    return false;
}

template<>
bool WrapperInstruction<Shift>::refine(
    ConnectionDB& conns) const
{
    auto s = new Split(input()->type());
    unsigned shftWidth = idxwidth(bitwidth(_ins->getOperand(0)->getType()));
    auto trunc = new IntTruncate(
        _ins->getOperand(1)->getType(),
        llvm::Type::getIntNTy(_ins->getContext(), shftWidth));
    conns.connect(s->dout(1), trunc->din());

    Shift* ret = NULL;
    switch(_ins->getOpcode()) {
    case llvm::Instruction::Shl:
        ret = new Shift(_ins->getOperand(0)->getType(),
                        Shift::Left,
                        Shift::Logical);
        break;
    case llvm::Instruction::LShr:
        ret = new Shift(_ins->getOperand(0)->getType(),
                        Shift::Right,
                        Shift::Logical);
        break;
    case llvm::Instruction::AShr:
        ret = new Shift(_ins->getOperand(0)->getType(),
                        Shift::Right,
                        Shift::Arithmetic);
        break;
    default:
        throw InvalidArgument("Don't know how to convert to shift!");
    }
    conns.connect(s->dout(0), ret->din(conns, 0));
    conns.connect(trunc->dout(), ret->din(conns, 1));
    conns.remap(output(), ret->dout());
    conns.remap(input(), s->din());
    return true;
}

template<>
bool WrapperInstruction<IntCompare>::refine(
    ConnectionDB& conns) const
{
    llvm::ICmpInst* icmp = llvm::dyn_cast<llvm::ICmpInst>(_ins);
    IntCompare::Cmp cmp;
    bool swapOps;
    bool isSigned;
    switch(icmp->getPredicate()) {
    case llvm::CmpInst::ICMP_EQ:
        cmp = IntCompare::EQ;
        swapOps = false;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_NE:
        cmp = IntCompare::NEQ;
        swapOps = false;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_UGT:
        cmp = IntCompare::GT;
        swapOps = false;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_UGE:
        cmp = IntCompare::GTE;
        swapOps = false;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_ULT:
        cmp = IntCompare::GT;
        swapOps = true;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_ULE:
        cmp = IntCompare::GTE;
        swapOps = true;
        isSigned = false;
        break;
    case llvm::CmpInst::ICMP_SGT:
        cmp = IntCompare::GT;
        swapOps = false;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SGE:
        cmp = IntCompare::GTE;
        swapOps = false;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SLT:
        cmp = IntCompare::GT;
        swapOps = true;
        isSigned = true;
        break;
    case llvm::CmpInst::ICMP_SLE:
        cmp = IntCompare::GTE;
        swapOps = true;
        isSigned = true;
        break;
    default:
        throw InvalidArgument("Don't know how to convert to IntCompare!");
    }

    if (!swapOps) {
        auto cmpBlk = new IntCompare(_ins->getOperand(0)->getType(),
                                     _ins->getOperand(1)->getType(),
                                     cmp, isSigned);
        conns.remap(input(), cmpBlk->din());
        conns.remap(output(), cmpBlk->dout());
    } else {
        auto cmpBlk = new IntCompare(_ins->getOperand(1)->getType(),
                                     _ins->getOperand(0)->getType(),
                                     cmp, isSigned);
        auto s = new Split(input()->type());
        conns.connect(cmpBlk->din(conns, 0), s->dout(1));
        conns.connect(cmpBlk->din(conns, 1), s->dout(0));
        conns.remap(input(), s->din());
        conns.remap(output(), cmpBlk->dout());
    }
    return true;
}

template<typename C>
bool WrapperInstruction<C>::refine(
    ConnectionDB& conns) const
{
    assert(_ins->getNumOperands() == 1);
    C* b = new C(GetInput(_ins), GetOutput(_ins));

    conns.remap(input(), b->din());
    conns.remap(output(), b->dout());
    return true;
}

template<typename Inner>
class TruncatingIntWrapperInstruction: public LLVMPureInstruction {
    Inner* New() const;

public:
    TruncatingIntWrapperInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMPureInstruction(bb, ins)
    { }

    static TruncatingIntWrapperInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        return new TruncatingIntWrapperInstruction(bb, ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const {
        Inner* b = New();
        conns.remap(input(), b->din());
        IntTruncate* trunc = new IntTruncate(b->dout()->type(), output()->type());
        conns.connect(b->dout(), trunc->din());
        conns.remap(output(), trunc->dout());
        return true;
    }
};

template<>
IntAddition* TruncatingIntWrapperInstruction<IntAddition>::New() const {
    std::vector<llvm::Type*> ops;
    for (unsigned i=0; i<_ins->getNumOperands(); i++) {
        if (!HWIgnoresOperand(_ins, i))
            ops.push_back(_ins->getOperand(i)->getType());
    }
    return new IntAddition(ops);
}

template<>
IntMultiply* TruncatingIntWrapperInstruction<IntMultiply>::New() const {
    std::vector<llvm::Type*> ops;
    for (unsigned i=0; i<_ins->getNumOperands(); i++) {
        if (!HWIgnoresOperand(_ins, i))
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
    IntWrapperInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMPureInstruction(bb, ins)
    { }

    static IntWrapperInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        return new IntWrapperInstruction(bb, ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const {
        Inner* b = New();
        conns.remap(input(), b->din());
        conns.remap(output(), b->dout());
        return true;
    }
};


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




class FlowInstruction: public LLVMPureInstruction {
public:
    FlowInstruction(const LLVMBasicBlock* bb, llvm::Instruction* ins) :
        LLVMPureInstruction(bb, ins, GetInput(ins), GetOutput(ins))
    { }

    static LLVMInstruction* Create(const LLVMBasicBlock* bb, llvm::Instruction* ins) {
        if (ins->getNumOperands() == 1)
            return new LLVMConstant(bb, ins);
        return new FlowInstruction(bb, ins);
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const
    {
        auto i = new Identity(this->din()->type());

        conns.remap(input(), i->din());
        conns.remap(output(), i->dout());

        return true;
    }
};

std::string LLVMMiscInstruction::print() const {
    return valuestr(_ins);
}

LLVMLoadInstruction::LLVMLoadInstruction(const LLVMBasicBlock* bb,
                                         llvm::Instruction* ins) :
        LLVMImpureInstruction(bb, ins, &_din),
        _din(this, GetInput(ins), "x"),
        _dout(this, GetOutput(ins), "a"),
        _readReq(this, GetInput(ins), "readReq"),
        _readResp(this, GetOutput(ins), "readResp") { }

bool LLVMLoadInstruction::refine(ConnectionDB& conns) const {
    auto inputID = new Identity(input()->type());
    conns.remap(input(), inputID->din());
    conns.remap(readReq(), inputID->dout());
    auto outputID = new Identity(output()->type());
    conns.remap(output(), outputID->dout());
    conns.remap(readResp(), outputID->din());
    return true;
}

LLVMInstruction* LLVMLoadInstruction::Create(
        const LLVMBasicBlock* bb, llvm::Instruction* ins) {
    if (isByvalLoad(ins)) 
        return WrapperInstruction<Identity>::Create(bb, ins);
    return new LLVMLoadInstruction(bb, ins);
}

bool LLVMLoadInstruction::isByval(llvm::Value* val) {
    if (llvm::Argument* arg = llvm::dyn_cast_or_null<llvm::Argument>(val)) {
        return arg->hasByValAttr();
    }
    return false;
}

bool LLVMLoadInstruction::isByvalLoad(llvm::Instruction* ins) {
    if (llvm::LoadInst* li = llvm::dyn_cast_or_null<llvm::LoadInst>(ins)) {
        return isByval(li->getOperand(0));
    }
    return false;
}

LLVMStoreInstruction::LLVMStoreInstruction(const LLVMBasicBlock* bb,
                                           llvm::Instruction* ins) :
    LLVMImpureInstruction(bb, ins, &_din),
    _din(this, GetInput(ins), "x"),
    _dout(this, GetOutput(ins), "a"),
    _writeReq(this, GetInput(ins), "writeReq"),
    _writeResp(this, GetOutput(ins), "writeResp") { }


bool LLVMStoreInstruction::refine(ConnectionDB& conns) const {
    auto inputID = new Identity(input()->type());
    conns.remap(input(), inputID->din());
    conns.remap(writeReq(), inputID->dout());
    auto outputID = new Identity(output()->type());
    conns.remap(output(), outputID->dout());
    conns.remap(writeResp(), outputID->din());
    return true;
}

typedef boost::function<LLVMInstruction* (const LLVMBasicBlock* bb, llvm::Instruction*)> InsConstructor;
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
    {llvm::Instruction::ICmp, WrapperInstruction<IntCompare>::Create},

    // Conversion operators
    {llvm::Instruction::Trunc, WrapperInstruction<IntTruncate>::Create},
    {llvm::Instruction::ZExt, WrapperInstruction<IntExtend>::Create},
    {llvm::Instruction::SExt, WrapperInstruction<IntExtend>::Create},
    {llvm::Instruction::InsertElement,
        WrapperInstruction<ReplaceElement>::Create},

    // Packing Operators
    {llvm::Instruction::ExtractElement, WrapperInstruction<Extract>::Create},

    // Logical operators (integer operands)
    {llvm::Instruction::Shl, WrapperInstruction<Shift>::Create}, // Shift left  (logical)
    {llvm::Instruction::LShr, WrapperInstruction<Shift>::Create}, // Shift right (logical)
    {llvm::Instruction::AShr, WrapperInstruction<Shift>::Create}, // Shift right (arithmetic)
    {llvm::Instruction::And, IntWrapperInstruction<Bitwise>::Create},
    {llvm::Instruction::Or, IntWrapperInstruction<Bitwise>::Create},
    {llvm::Instruction::Xor, IntWrapperInstruction<Bitwise>::Create},

    // Control flow operators
    {llvm::Instruction::Ret, WrapperInstruction<Identity>::Create},
    {llvm::Instruction::Br, FlowInstruction::Create},
    {llvm::Instruction::Switch, FlowInstruction::Create},

    // Memory ops
    {llvm::Instruction::Load, LLVMLoadInstruction::Create},
    {llvm::Instruction::Store, LLVMStoreInstruction::Create},
};

LLVMInstruction* LLVMInstruction::Create(const LLVMBasicBlock* bb, llvm::Instruction* ins) {
    assert(ins != NULL);
    auto f = Constructors.find(ins->getOpcode());
    if (f == Constructors.end())
        return new LLVMMiscInstruction(bb, ins);
    return f->second(bb, ins);
}

} // namespace llpm
