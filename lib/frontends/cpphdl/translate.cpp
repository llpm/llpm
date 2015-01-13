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
    cout << "Class " << className << ": " << typestr(classType) << endl;

    CPPHDLClass* chClass = new CPPHDLClass(
        _design, className, classType, lm);

    string fnPrefix = className + "::";
    string testStr = "test";
    for (auto&& func: lm->getFunctionList()) {
        auto demangledName = cpp_demangle(func.getName().str().c_str());
        if (demangledName.find(fnPrefix) == 0) {
            auto fnNameArgs = demangledName.substr(fnPrefix.size());
            auto fnName = fnNameArgs;
            auto parenLoc = fnNameArgs.find_first_of('(');
            if (parenLoc != string::npos) {
                fnName = fnNameArgs.substr(0, parenLoc);
            }

            if (fnName== className)
                // Found a constructor! Skip them.
                continue;

            printf("Found class member: %s\n", demangledName.c_str());
            chClass->addMember(
                fnName,
                &func,
                _llvmTranslator.translate(&func)
                );
        } else if (demangledName.find(testStr) == 0) {
            // This member is a test. Don't generate HW, but a
            // test function instead
            printf("Found test: %s\n", demangledName.c_str());
            chClass->adoptTest(&func);
        }
    }

    return chClass;
}

} // namespace cpphdl
} // namespace llpm
