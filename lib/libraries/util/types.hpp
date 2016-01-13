#pragma once

#include <llpm/block.hpp>

#include <vector>

namespace llpm {

/**
 * Reorders the fields of a struct with a statically determined
 * mapping of input fields to output fields.
 */
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

/**
 * Analog to LLVM's insertvalue instruction. Replaces one field of a
 * struct or vector with a replacement value. If a vector, the field
 * can be dynamically selected. 
 */
class ReplaceElement : public Function {
    signed _idx;

    static llvm::Type* GetInput(llvm::Type* input, signed idx);

public:
    /**
     * Create a replacement with a statically selected index
     */
    ReplaceElement(llvm::Type* ty, signed idx);
    /**
     * Create a replacement with a dynamically selected index
     */
    ReplaceElement(llvm::Type* ty);

    DEF_GET_NP(idx);

    virtual bool refinable() const {
        return false;
    }

    virtual bool refine(ConnectionDB& conns) const;
};

} // namespace llpm

