#ifndef __LLPM_CPPHDL_OBJECTS_HPP__
#define __LLPM_CPPHDL_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <libraries/util/types.hpp>
#include <frontends/llvm/objects.hpp>

// Fwd. defs. Why am I basing this language on such a stupid language?
namespace llvm {
    class StructType;
}

namespace llpm {
namespace cpphdl {

class CPPHDLClass : public ContainerModule {
    friend class CPPHDLTranslator;

    CPPHDLClass(Design& design,
                std::string name,
                llvm::StructType* ty);

    void addMember(std::string name, LLVMFunction*);
};

}
}

#endif // __LLPM_CPPHDL_OBJECTS_HPP__

