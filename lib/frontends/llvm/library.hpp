#ifndef __LLPM_LLVM_LIBRARY_HPP__
#define __LLPM_LLVM_LIBRARY_HPP__

#include <frontends/llvm/objects.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

class LLVMBaseLibrary : public Refinery::Library {
    static std::vector<std::shared_ptr<Refinery::Refiner>> BuildCollection();
public:
    LLVMBaseLibrary() :
        Refinery::Library(BuildCollection())
    { }
    virtual ~LLVMBaseLibrary() { }

    virtual std::string name() {
        return "LLVM base translation library";
    }
};

template<typename T>
class LLVMRefiner : public BlockRefiner {
public:
    virtual bool handles(Block* b) const {
        return dynamic_cast<T*>(b) != NULL;
    }
};

} // namespace llpm

#endif // __LLPM_LLVM_LIBRARAY_HPP__
