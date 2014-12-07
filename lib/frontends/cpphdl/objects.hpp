#ifndef __LLPM_CPPHDL_OBJECTS_HPP__
#define __LLPM_CPPHDL_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <libraries/util/types.hpp>

namespace llpm {
namespace cpphdl {

class CPPHDLClass : public ContainerModule {
    friend class CPPHDLTranslator;
};

}
}

#endif // __LLPM_CPPHDL_OBJECTS_HPP__

