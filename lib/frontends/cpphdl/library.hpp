#ifndef __LLPM_CPPHDL_LIBRARY_HPP__
#define __LLPM_CPPHDL_LIBRARY_HPP__

#include <frontends/cpphdl/objects.hpp>
#include <refinery/refinery.hpp>

namespace llpm {
namespace cpphdl {
    
class CPPHDLBaseLibrary : public Refinery::Library {
    static std::vector<std::shared_ptr<Refinery::Refiner>> BuildCollection();
public:
    CPPHDLBaseLibrary() :
        Refinery::Library(BuildCollection())
    { }
    virtual ~CPPHDLBaseLibrary() { }

    virtual std::string name() {
        return "CPPHDL translation library";
    }
};

}
}
#endif // __LLPM_CPPHDL_LIBRARY_HPP__
