#include "translate.hpp"
#include <frontends/cpphdl/library.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Module.h>

using namespace std;

namespace llpm {
namespace cpphdl {

CPPHDLTranslator::CPPHDLTranslator(Design& design) :
    _design(design),
    _llvmTranslator(design) {
    design.refinery().appendLibrary(new CPPHDLBaseLibrary());
}

CPPHDLTranslator::~CPPHDLTranslator() {
}

CPPHDLClass* CPPHDLTranslator::translate(std::string className) {
    llvm::Module* lm = _llvmTranslator.getModule();

    llvm::StructType* classType = lm->getTypeByName("class." + className);
    if (classType == NULL) {
        throw InvalidArgument("Could not find class type!");
    }
    cout << "Class: " << typestr(classType) << endl;

    CPPHDLClass* chClass = new CPPHDLClass(_design, className, classType);

    string fnPrefix = className + "::";
    for (auto&& func: lm->getFunctionList()) {
        auto demangledName = cpp_demangle(func.getName().str().c_str());
        if (demangledName.find(fnPrefix) == 0) {
            printf("Found class member: %s\n", demangledName.c_str());
            chClass->addMember(
                demangledName.substr(fnPrefix.size()),
                _llvmTranslator.translate(&func)
                );
        } else {
            printf("Rejected function: %s\n", demangledName.c_str());
        }
    }

    return chClass;
}

} // namespace cpphdl
} // namespace llpm