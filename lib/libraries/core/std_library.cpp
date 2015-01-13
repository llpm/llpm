#include "std_library.hpp"

#include <util/llvm_type.hpp>
#include <util/misc.hpp>
#include <algorithm>
#include <llpm/exceptions.hpp>
#include <boost/foreach.hpp>
#include <llvm/IR/DerivedTypes.h>

using namespace std;

namespace llpm {
using namespace llvm;

void StdLibStops(BaseLibraryStopCondition& sc) {
    sc.addClass<IntAddition>();
    sc.addClass<IntSubtraction>();
    sc.addClass<ConstShift>();
    sc.addClass<Shift>();
    sc.addClass<IntMultiply>();
    sc.addClass<IntDivide>();
    sc.addClass<IntRemainder>();
    sc.addClass<Bitwise>();
    sc.addClass<IntCompare>();
    sc.addClass<IntTruncate>();
    sc.addClass<IntExtend>();

    sc.addClass<Constant>();
    sc.addClass<Never>();
    sc.addClass<BooleanLogic>();
    sc.addClass<CommunicationIntrinsic>();
}

llvm::Type* IntAddition::InType(std::vector<llvm::Type*> tVec) {
    if (tVec.size() == 0)
        throw InvalidArgument("Type vector must not be empty");
    for(auto&& t: tVec) {
        if (!t->isIntegerTy())
            throw InvalidArgument("All inputs to Int* blocks must be ints!");
    }
    return StructType::get(tVec[0]->getContext(), tVec);
}

llvm::Type* IntAddition::OutType(std::vector<llvm::Type*> tVec) {
    vector<unsigned> widths;
    for(auto&& t: tVec) {
        if (!t->isIntegerTy())
            throw InvalidArgument("All inputs to IntAddition must be ints!");
        unsigned tw = t->getScalarSizeInBits();
        widths.push_back(tw);
    }

    sort(widths.begin(), widths.end());
    unsigned width = 0;
    for(auto&& tw: widths) {
        if (width == 0)
            width = tw;
        else if (tw > width)
            width = tw + 1;
        else
            width = width + 1;
    }
    return Type::getIntNTy(tVec[0]->getContext(), width);
}

IntAddition::IntAddition(std::vector<llvm::Type*> t) :
    Function(InType(t), OutType(t))
{
}

IntSubtraction::IntSubtraction(llvm::Type* a, llvm::Type* b) :
    Function(IntAddition::InType({a, b}), IntAddition::OutType({a, b}))
{
}

llvm::Type* ConstShift::InType(llvm::Type* a, int shift, Style style) {
    if (!a->isIntegerTy())
        throw InvalidArgument("All inputs to IntAddition must be ints!");
    return a;
}

llvm::Type* ConstShift::OutType(llvm::Type* a, int shift, Style style) {
    if (!a->isIntegerTy())
        throw InvalidArgument("All inputs to IntAddition must be ints!");

    bool trunc = style == LogicalTruncating;
    int ashift = (shift > 0) ? shift : -1 * shift;
    if (a->getScalarSizeInBits() < ashift)
        throw InvalidArgument("Cannot shift more than the width of the type!");

    if ((trunc && shift > 0) || (!trunc && shift < 0) || shift == 0)
        return a;
    else
        return Type::getIntNTy(a->getContext(), a->getScalarSizeInBits() + shift);
}

ConstShift::ConstShift(llvm::Type* a, int shift, Style style)  :
    Function(InType(a, shift, style), OutType(a, shift, style)),
    _shift(shift),
    _style(style)
{
}

llvm::Type* Shift::InType(llvm::Type* a, Direction dir, Style Style) {
    return llvm::StructType::get(a->getContext(), vector<llvm::Type*>(
        {a, llvm::Type::getIntNTy(a->getContext(), idxwidth(bitwidth(a)))}));
}

llvm::Type* Shift::OutType(llvm::Type* a, Direction dir, Style Style) {
    if (!a->isIntegerTy())
        throw InvalidArgument("All inputs to IntAddition must be ints!");
    return a;
}

Shift::Shift(llvm::Type* a, Direction dir, Style style) :
    Function(InType(a, dir, style), OutType(a, dir, style)),
    _dir(dir),
    _style(style)
{
}

llvm::Type* IntTruncate::InType(unsigned N, llvm::Type* t) {
    if (!t->isIntegerTy())
        throw InvalidArgument("Input to IntTruncate must be int!");
    return t;
}

llvm::Type* IntTruncate::OutType(unsigned N, llvm::Type* t) {
    if (!t->isIntegerTy())
        throw InvalidArgument("Input to IntTruncate must be int!");
    if (N > t->getScalarSizeInBits())
        throw InvalidArgument("Cannot truncate by more bits than input has!");

    return llvm::Type::getIntNTy(t->getContext(), t->getScalarSizeInBits() - N);
}

IntTruncate::IntTruncate(unsigned N, llvm::Type* t) :
    Function(InType(N, t), OutType(N, t))
{ }

IntTruncate::IntTruncate(llvm::Type* a, llvm::Type* b) :
    Function(a, b)
{
    if (!a->isIntegerTy() || !b->isIntegerTy())
        throw InvalidArgument("Input and outputs to IntTruncate must be ints!");
    if (a->getScalarSizeInBits() < b->getScalarSizeInBits())
        throw InvalidArgument("Input to IntTrunc must be wider than output!");
}

llvm::Type* IntExtend::InType(unsigned N, llvm::Type* t) {
    if (!t->isIntegerTy())
        throw InvalidArgument("Input to IntExtend must be int!");
    return t;
}

llvm::Type* IntExtend::OutType(unsigned N, llvm::Type* t) {
    if (!t->isIntegerTy())
        throw InvalidArgument("Input to IntExtend must be int!");

    return llvm::Type::getIntNTy(t->getContext(), t->getScalarSizeInBits() + N);
}

IntExtend::IntExtend(unsigned N, bool signExtend, llvm::Type* t) :
    Function(InType(N, t), OutType(N, t)),
    _signExtend(signExtend)
{ }

llvm::Type* IntMultiply::InType(std::vector<llvm::Type*> t) {
    return IntAddition::InType(t);
}

llvm::Type* IntMultiply::OutType(std::vector<llvm::Type*> tVec) {
    if (tVec.size() == 0)
        throw InvalidArgument("There must be at least one input to IntMultiply");
    unsigned width = 0;
    for(auto&& t: tVec) {
        if (!t->isIntegerTy())
            throw InvalidArgument("All inputs to IntMultiply must be ints!");
        width += t->getScalarSizeInBits();
    }
    return Type::getIntNTy(tVec[0]->getContext(), width);
}

IntMultiply::IntMultiply(std::vector<llvm::Type*> t) :
    Function(InType(t), OutType(t))
{
}

llvm::Type* IntDivide::InType(llvm::Type* a, llvm::Type* b, bool isSigned) {
    return IntAddition::InType({a, b});
}

llvm::Type* IntDivide::OutType(llvm::Type* a, llvm::Type* b, bool isSigned) {
    if (!a->isIntegerTy() || !b->isIntegerTy())
        throw InvalidArgument("All inputs to IntDivide must be ints!");
    return a;
}

IntDivide::IntDivide(llvm::Type* a, llvm::Type* b, bool isSigned) :
    Function(InType(a, b, isSigned), OutType(a, b, isSigned)),
    _isSigned(isSigned)
{
}

llvm::Type* IntRemainder::InType(llvm::Type* a, llvm::Type* b, bool isSigned) {
    return IntAddition::InType({a, b});
}

llvm::Type* IntRemainder::OutType(llvm::Type* a, llvm::Type* b, bool isSigned) {
    if (!a->isIntegerTy() || !b->isIntegerTy())
        throw InvalidArgument("All inputs to IntDivide must be ints!");
    return b;
}

IntRemainder::IntRemainder(llvm::Type* a, llvm::Type* b, bool isSigned) :
    Function(InType(a, b, isSigned), OutType(a, b, isSigned)),
    _isSigned(isSigned)
{
}

llvm::Type* Bitwise::InType(unsigned N, llvm::Type* t) {
    if (N == 0)
        throw InvalidArgument("Bitwise must have at least one input!");
    if (!t->isIntegerTy())
        throw InvalidArgument("All inputs to Bitwise must be ints!");
    vector<llvm::Type*> sig;
    for (unsigned i=0; i<N; i++)
        sig.push_back(t);
    return StructType::get(t->getContext(), sig);
}

llvm::Type* Bitwise::OutType(unsigned N, llvm::Type* t) {
    if (N == 0)
        throw InvalidArgument("Bitwise must have at least one input!");
    if (!t->isIntegerTy())
        throw InvalidArgument("All inputs to Bitwise must be ints!");
    return t;
}

Bitwise::Bitwise(unsigned N, llvm::Type* t, Op op) :
    Function(InType(N, t), OutType(N, t)),
    _op(op)
{
}

llvm::Type* IntCompare::InType(llvm::Type* a, llvm::Type* b,
                               Cmp op, bool isSigned)
{
    return IntAddition::InType({a, b});
}

llvm::Type* IntCompare::OutType(llvm::Type* a, llvm::Type* b,
                                   Cmp op, bool isSigned)
{
    return Type::getInt1Ty(a->getContext());
}
IntCompare::IntCompare(llvm::Type* a, llvm::Type* b,
                       Cmp op, bool isSigned) :
    Function(InType(a, b, op, isSigned), OutType(a, b, op, isSigned)),
    _op(op),
    _isSigned(isSigned)
{
}

} // namespace llpm

