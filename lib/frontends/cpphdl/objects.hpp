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
    class GetElementPtrInst;
}

namespace llpm {
namespace cpphdl {

class CPPHDLClass : public ContainerModule {
    friend class CPPHDLTranslator;

    // My base type
    llvm::StructType* _type;

    // Different member variable types
    std::map<unsigned, Register*>  _regs;
    std::map<unsigned, FiniteArray*>     _arrays;
    std::map<unsigned, Module*>    _modules;

    // Read and write ports may require interface multiplexers
    std::map<unsigned, InterfaceMultiplexer*> _writePorts;
    std::map<unsigned, InterfaceMultiplexer*> _readPorts;

    // All member variables, in order
    std::vector<Block*>            _variables;
    // All member methods, in order
    std::map<std::string, Module*> _methods;

    CPPHDLClass(Design& design,
                std::string name,
                llvm::StructType* ty);

    void addMember(std::string name, LLVMFunction*);
    void connectMem(LLVMFunction*);
    unsigned resolveMember(llvm::Value*) const;
    void buildVariables();
    Block* buildVariable(unsigned i);
};

}
}

#endif // __LLPM_CPPHDL_OBJECTS_HPP__

