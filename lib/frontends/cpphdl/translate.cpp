#include "translate.hpp"
#include "library.hpp"

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
    return NULL;
}

} // namespace cpphdl
} // namespace llpm
