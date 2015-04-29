#ifndef __LLPM_LLVM_TRANSLATE_HPP__
#define __LLPM_LLVM_TRANSLATE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>

// fwd def
namespace llvm {
    class Module;
}

namespace llpm {

class LLVMTranslator {
    Design& _design;
    llvm::Module* _llvmModule;
    std::map<llvm::Function*, llvm::Function*> _origToPrepared;
    std::set<llvm::Function*> _toPrepare;

public:
    LLVMTranslator(Design& design);
    ~LLVMTranslator();

    void readBitcode(std::string fileName);
    void setModule(llvm::Module* module);
    llvm::Module* getModule() {
        return _llvmModule;
    }

    void prepare(llvm::Function*);
    void prepare(std::string fnName);

    virtual void translate();

    LLVMFunction* get(llvm::Function*);
    LLVMFunction* get(std::string fnName);

private:
    void optimize(llvm::Module* module);
    llvm::Function* elevateArgs(llvm::Function*);
};

} // namespace llpm

#endif // __LLPM_LLVM_TRANSLATE_HPP__
