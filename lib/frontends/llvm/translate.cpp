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
    setModule(_design.readBitcode(fileName));
    _pm = new llvm::legacy::FunctionPassManager(_llvmModule);
    _pm->add(llvm::createInstructionNamerPass());
}

LLVMFunction* LLVMTranslator::translate(llvm::Function* func) {
    if (func == NULL)
        throw InvalidArgument("Function cannot be NULL!");
    return new LLVMFunction(this->_design, func);
}

LLVMFunction* LLVMTranslator::translate(std::string fnName) {
    if (this->_llvmModule == NULL)
        throw InvalidCall("Must load a module into LLVMTranslator before translating");
    llvm::Function* func = this->_llvmModule->getFunction(fnName);
    if (func == NULL)
        throw InvalidArgument("Could not find function: " + fnName);
    _pm->run(*func);
    return translate(func);
}

} // namespace llpm

