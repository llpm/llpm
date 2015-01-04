#include "llvm_type.hpp"
#include <llpm/exceptions.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/format.hpp>

namespace llpm {

unsigned bitwidth(llvm::Type* t) {
    unsigned size = t->getPrimitiveSizeInBits();
    if (size > 0 || t->isVoidTy())
        return size;

    size = 0;
    llvm::CompositeType* ct = llvm::dyn_cast<llvm::CompositeType>(t);
    if (ct == NULL)
        throw InvalidArgument("bitwidth can only be calculated for scalar and composite types!");

    for (unsigned i=0; i<ct->getNumContainedTypes(); i++) {
        size += bitwidth(ct->getContainedType(i));
    }

    return size;
}

unsigned bitoffset(llvm::Type* ct, unsigned n) {
    unsigned acc = 0;
    if (n >= ct->getNumContainedTypes())
        throw InvalidArgument("n must index into composite type and not beyond");
    for (unsigned i=0; i<n; i++) {
        acc += bitwidth(ct->getContainedType(i));
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

}

