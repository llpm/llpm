#ifndef __LLPM_UTIL_TYPES_HPP__
#define __LLPM_UTIL_TYPES_HPP__

#include <llpm/block.hpp>

#include <vector>

namespace llpm {

// Reorders the fields of a struct
class StructTwiddler : public Function {
private:
    std::vector<unsigned> _mapping;

    static llvm::Type* GetOutput(llvm::Type* input,
                                 const std::vector<unsigned>& mapping);

public:
    StructTwiddler(llvm::Type* input, const std::vector<unsigned>& mapping);
    virtual ~StructTwiddler() { }

    virtual bool refinable() const {
        return false;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        ConnectionDB& conns) const;
};

} // namespace llpm

#endif // __LLPM_UTIL_TYPES_HPP__
