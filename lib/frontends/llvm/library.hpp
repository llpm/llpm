#ifndef __LLPM_LLVM_LIBRARY_HPP__
#define __LLPM_LLVM_LIBRARY_HPP__

#include <frontends/llvm/objects.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMBaseLibrary : public Refinery<Block>::Library {
    static std::vector<Refinery<Block>::Refiner*> BuildCollection();
public:
    LLVMBaseLibrary() :
        Refinery<Block>::Library(BuildCollection())
    { }
    virtual ~LLVMBaseLibrary() { }

    virtual std::string name() {
        return "LLVM base translation library";
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_LIBRARAY_HPP__
