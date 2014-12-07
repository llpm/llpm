#ifndef __LLPM_LLVM_TRANSLATE_HPP__
#define __LLPM_LLVM_TRANSLATE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>

// fwd def
namespace llvm {
    class Module;
    namespace legacy {
        class FunctionPassManager;
    }
}

namespace llpm {

class LLVMTranslator {
    Design& _design;
    std::auto_ptr < llvm::Module > _llvmModule;
    llvm::legacy::FunctionPassManager* _pm;

public:
    LLVMTranslator(Design& design);
    ~LLVMTranslator();

    void readBitcode(std::string fileName);
    void setModule(llvm::Module* module) {
        assert(module != NULL);
        this->_llvmModule.reset(module);
    }
    llvm::Module* getModule() {
        return _llvmModule.get();
    }

    LLVMFunction* translate(llvm::Function*);
    LLVMFunction* translate(std::string fnName);
};

} // namespace llpm

#endif // __LLPM_LLVM_TRANSLATE_HPP__
