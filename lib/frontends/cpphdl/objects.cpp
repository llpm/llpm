#include "objects.hpp"
#include <frontends/cpphdl/library.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Module.h>

using namespace std;

namespace llpm {
namespace cpphdl {

CPPHDLClass::CPPHDLClass(Design& design,
                         std::string name,
                         llvm::StructType* ty) :
    ContainerModule(design, name),
    _type(ty) {
    buildVariables();
}

void CPPHDLClass::addMember(std::string name, LLVMFunction* func) {
    addServerInterface(func->call()->din(), func->call()->dout(), name);
    _methods[name] = func;
}

void CPPHDLClass::buildVariables() {
    for (unsigned i=0; i<_type->getNumElements(); i++) {
        _variables.push_back(buildVariable(i));
    }
}

Block* CPPHDLClass::buildVariable(unsigned i) {
    assert(i < _type->getNumElements());
    llvm::Type* ty = _type->getElementType(i);

    if (ty->isPointerTy()) {
        // Build a link to an external module
        assert(false && "Not supported yet");
    } else if (ty->isSingleValueType() && !ty->isPointerTy()) {
        // Build a register
        Register* reg = new Register(ty);
        _regs[i] = reg;
        return reg;
    } else if (ty->isArrayTy()) {
        // Build a RAM
        llvm::ArrayType* arrTy = llvm::dyn_cast<llvm::ArrayType>(ty);
        Array* array = new Array(arrTy->getElementType(),
                                 arrTy->getNumElements());
        _arrays[i] = array;
        return array;
    } else {
        throw InvalidArgument("Cannot determine how to implement type: " +
                              typestr(ty));
    }
}

} // namespace cpphdl
} // namespace llpm
