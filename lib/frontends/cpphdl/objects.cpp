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
    ContainerModule(design, name) {
}

void CPPHDLClass::addMember(std::string name, LLVMFunction* func) {
    addInputPort(func->call(), name + "_call");
    addOutputPort(func->ret(), name + "_ret");
}

} // namespace cpphdl
} // namespace llpm
