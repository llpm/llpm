#ifndef __LLPM_UTIL_TYPES_HPP__
#define __LLPM_UTIL_TYPES_HPP__

#include <llpm/block.hpp>

#include <vector>

namespace llpm {

// Reorders the fields of a struct
class StructTwiddler : public Function {
    std::vector<unsigned> _mapping;

    static llvm::Type* GetOutput(llvm::Type* input,
                                 const std::vector<unsigned>& mapping);

public:
    StructTwiddler(llvm::Type* input, const std::vector<unsigned>& mapping);
    virtual ~StructTwiddler() { }

    virtual bool refinable() const {
        return false;
    }

    virtual bool refine(ConnectionDB& conns) const;
};

class ReplaceElement : public Function {
    signed _idx;

    static llvm::Type* GetInput(llvm::Type* input, signed idx);

public:
    // Statically selected index
    ReplaceElement(llvm::Type* ty, signed idx);
    // Dynamically selected index
    ReplaceElement(llvm::Type* ty);

    DEF_GET_NP(idx);

    virtual bool refinable() const {
        return false;
    }

    virtual bool refine(ConnectionDB& conns) const;
};

} // namespace llpm

#endif // __LLPM_UTIL_TYPES_HPP__
