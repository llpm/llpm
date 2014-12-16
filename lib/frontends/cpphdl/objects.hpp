#ifndef __LLPM_CPPHDL_OBJECTS_HPP__
#define __LLPM_CPPHDL_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <libraries/util/types.hpp>
#include <libraries/core/mem_intr.hpp>
#include <frontends/llvm/objects.hpp>

// Fwd. defs. Why am I basing this language on such a stupid language?
namespace llvm {
    class StructType;
}

namespace llpm {
namespace cpphdl {

class CPPHDLClass : public ContainerModule {
    friend class CPPHDLTranslator;

    // My base type
    llvm::StructType* _type;

    // Different member variable types
    std::map<unsigned, Register*>  _regs;
    std::map<unsigned, Array*>     _arrays;
    std::map<unsigned, Module*>    _modules;

    // All member variables, in order
    std::vector<Block*>            _variables;
    // All member methods, in order
    std::map<std::string, Module*> _methods;

    CPPHDLClass(Design& design,
                std::string name,
                llvm::StructType* ty);

    void addMember(std::string name, LLVMFunction*);
    void buildVariables();
    Block* buildVariable(unsigned i);
};

}
}

#endif // __LLPM_CPPHDL_OBJECTS_HPP__

