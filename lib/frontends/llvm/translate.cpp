#include "translate.hpp"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>

#include <llvm/Transforms/Scalar.h>

#include <frontends/llvm/library.hpp>

namespace llpm {

LLVMTranslator::LLVMTranslator(Design& design) :
    _design(design) {
    design.refinery().appendLibrary(new LLVMBaseLibrary());
}

LLVMTranslator::~LLVMTranslator() {
}

void LLVMTranslator::readBitcode(std::string fileName) {
    llvm::PassRegistry *Registry = llvm::PassRegistry::getPassRegistry();
    initializeCore(*Registry);
    initializeAnalysis(*Registry);
    // initializeBasicCallGraphPass(*Registry);
    initializeScalarOpts(*Registry);

    llvm::SMDiagnostic Err;
    llvm::Module *mod = NULL;

    _llvmModule.reset(llvm::ParseIRFile(fileName, Err, _design.context()));
    mod = _llvmModule.get();
    if (mod == NULL) {
            Err.print("LLVMTranslator::readBitcode", llvm::errs());
            throw Exception("Could not parse module\n");
    }

    _pm = new llvm::legacy::FunctionPassManager(mod);
    _pm->add(llvm::createInstructionNamerPass());
}

LLVMFunction* LLVMTranslator::translate(llvm::Function* func) {
    if (func == NULL)
        throw InvalidArgument("Function cannot be NULL!");
    return new LLVMFunction(this->_design, func);
}

LLVMFunction* LLVMTranslator::translate(std::string fnName) {
    if (this->_llvmModule.get() == NULL)
        throw InvalidCall("Must load a module into LLVMTranslator before translating");
    llvm::Function* func = this->_llvmModule->getFunction(fnName);
    _pm->run(*func);
    if (func == NULL)
        throw InvalidArgument("Could not find function: " + fnName);
    return translate(func);
}

} // namespace llpm

