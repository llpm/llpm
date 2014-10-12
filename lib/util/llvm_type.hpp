#ifndef __LLPM_UTIL_LLVM_TYPE_HPP__
#define __LLPM_UTIL_LLVM_TYPE_HPP__

#include <string>

// If I never see another fwd declaration...
namespace llvm {
    class Type;
};

namespace llpm {

    // Compute the number of bits required to store or transmit a
    // certain type
    unsigned bitwidth(llvm::Type* t);

    // Compute the offset of the n-th type in a composite type
    unsigned bitoffset(llvm::Type* t, unsigned n);

    std::string typestr(llvm::Type* t);
}

#endif // __LLPM_UTIL_LLVM_TYPE_HPP__
