#include "llvm_type.hpp"
#include <llpm/exceptions.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/ValueHandle.h>

#include <boost/format.hpp>

namespace llvm {
    // This appears to be around for no reason other than to create
    // a typeinfo for CallbackVH. It doesn't create one in the LLVM
    // library since LLVM is no-rtti. For some reason, however, I need
    // the typeinfo even though I'm not directly extending CallbackVH.
    // Probably an LLVM class I'm using is in a template or something
    // stupid.
    // 
    // Anyway, LLVM is so painfully non-standard that I have to put
    // this here to avoid linking issues.
    void CallbackVH::anchor() {}
}

namespace llpm {

unsigned bitwidth(llvm::Type* t) {
    unsigned size = t->getPrimitiveSizeInBits();
    if (size > 0 || t->isVoidTy())
        return size;

    if (t->isPointerTy())
        return 64;

    size = 0;
    llvm::CompositeType* ct = llvm::dyn_cast<llvm::CompositeType>(t);
    if (ct == NULL)
        throw InvalidArgument("bitwidth can only be calculated for scalar and composite types!");

    for (unsigned i=0; i<numContainedTypes(ct); i++) {
        size += bitwidth(nthType(ct, i));;
    }

    return size;
}

unsigned bitoffset(llvm::Type* ct, unsigned n) {
    unsigned acc = 0;
    if (n >= numContainedTypes(ct))
        throw InvalidArgument("n must index into composite type and not beyond");
    for (unsigned i=0; i<n; i++) {
        acc += bitwidth(nthType(ct, i));
    }
    return acc;
}

std::string typestr(llvm::Type* t) {
    std::string s;
    llvm::raw_string_ostream os(s);
    t->print(os);
    return s;
}

std::string valuestr(llvm::Value* t) {
    std::string s;
    llvm::raw_string_ostream os(s);
    t->print(os);
    return s;
}

std::string name(llvm::Instruction* ins) {
    if (ins->hasName())
        return ins->getName().str();
    unsigned icount = 0;
    for (const auto& i: ins->getParent()->getInstList()) {
        if (ins == &i)
            break;
        icount++;
    }
    return str(boost::format("%1%_i%2%")
                % name(ins->getParent())
                % icount);
}

std::string name(llvm::BasicBlock* bb) {
    if (bb->hasName())
        return bb->getName().str();

    unsigned bbcount = 0;
    for (const auto& b: bb->getParent()->getBasicBlockList()) {
        if (bb== &b)
            break;
        bbcount++;
    }
    return str(boost::format("bb%1%")
                % bbcount);
}

llvm::Type* nthType(llvm::Type* ty, unsigned i) {
    assert(i < numContainedTypes(ty));
    switch(ty->getTypeID()) {
    case llvm::Type::StructTyID:
        return ty->getStructElementType(i);
    case llvm::Type::VectorTyID:
        if (i > ty->getVectorNumElements())
            throw InvalidArgument("i is out of bounds for this type!");
        return ty->getVectorElementType();
    case llvm::Type::ArrayTyID:
        if (i > ty->getVectorNumElements())
            throw InvalidArgument("i is out of bounds for this type!");
        return ty->getArrayElementType();
    default:
        throw InvalidArgument("Type does not appear to be a CompositeType!");
    }
}

unsigned numContainedTypes(llvm::Type* ty) {
    switch(ty->getTypeID()) {
    case llvm::Type::StructTyID:
        return ty->getStructNumElements();
    case llvm::Type::VectorTyID:
        return ty->getVectorNumElements();
    case llvm::Type::ArrayTyID:
        return ty->getVectorNumElements();
    default:
        return 0;
    }
}

}

