#include "translate.hpp"
#include <frontends/cpphdl/library.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Module.h>

using namespace std;

namespace llpm {
namespace cpphdl {

CPPHDLTranslator::CPPHDLTranslator(Design& design) :
    LLVMTranslator(design),
    _design(design) {
    design.refinery().appendLibrary(make_shared<CPPHDLBaseLibrary>());
}

CPPHDLTranslator::~CPPHDLTranslator() {
}

void CPPHDLTranslator::prepare(std::string className) {
    llvm::Module* lm = getModule();

    llvm::StructType* classType = lm->getTypeByName("class." + className);
    if (classType == NULL) {
        throw InvalidArgument("Could not find class type!");
    }
    cout << "Class " << className << ": " << typestr(classType) << endl;

    CPPHDLClass* chClass = new CPPHDLClass(
        _design, className, classType, lm);
    _classes[className] = chClass;

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

            if (fnName == className)
                // Found a constructor! Skip them.
                continue;

            if (func.isDeclaration())
                // No body! skip!
                continue;

            printf("Found class member: %s\n", demangledName.c_str());
            _classMethods[chClass].insert(make_pair(fnName, &func));
            LLVMTranslator::prepare(&func);
        } else if (demangledName.find(testStr) == 0) {
            // This member is a test. Don't generate HW, but a
            // test function instead
            printf("Found test: %s\n", demangledName.c_str());
            chClass->adoptTest(&func);
        }
    }
}

void CPPHDLTranslator::translate() {
    LLVMTranslator::translate();
}

CPPHDLClass* CPPHDLTranslator::get(std::string className) {
    CPPHDLClass* chClass = _classes[className];
    if (chClass == NULL) {
        throw InvalidArgument("Must call prepare for each class before you can get it!");
    }
        
    for (std::pair<std::string, llvm::Function*> p: _classMethods[chClass]) {
        std::string name = p.first;
        llvm::Function* func = p.second;
        chClass->addMember(
            name,
            func,
            LLVMTranslator::get(func)
            );
    }

    return chClass;
}

} // namespace cpphdl
} // namespace llpm
