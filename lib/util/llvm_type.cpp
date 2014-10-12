#include "llvm_type.hpp"
#include <llpm/exceptions.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/raw_ostream.h>

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

}

