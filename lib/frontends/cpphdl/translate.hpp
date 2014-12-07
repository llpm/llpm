#ifndef __LLPM_CPPHDL_TRANSLATE_HPP__
#define __LLPM_CPPHDL_TRANSLATE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/cpphdl/objects.hpp>
#include <frontends/llvm/translate.hpp>

namespace llpm {
namespace cpphdl {

class CPPHDLTranslator {
    Design& _design;
    LLVMTranslator _llvmTranslator;

public:
    CPPHDLTranslator(Design& design);
    ~CPPHDLTranslator();

    void readBitcode(std::string fileName) {
        _llvmTranslator.readBitcode(fileName);
    }
    void setModule(llvm::Module* module) {
        _llvmTranslator.setModule(module);
    }

    CPPHDLClass* translate(std::string className);
};

} // namespace cpphdl
} // namespace llpm

#endif // __LLPM_CPPHDL_TRANSLATE_HPP__
