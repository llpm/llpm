#ifndef __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__
#define __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>
#include <libraries/core/interface.hpp>

namespace llpm {

// Accepts an input on the write interface and stores it. Outputs the
// stored input when a request arrives on a read interface. Has separate
// read/write ports so that reads are not serialized.
class Register : public Block {
    Interface _write;
    std::vector<Interface*> _read;

public:
    Register(llvm::Type* type);
    virtual ~Register();

    virtual bool hasState() const {
        return true;
    }

    DEF_GET(write);
    DEF_ARRAY_GET(read);

    Interface* newRead();
};

// RAM. Has one write port and one read port which can be multiplexed.
// Later technology-specific optimizations can instantiate RAMs with
// different R/W port configurations and optimize the connectivity.
// Potentially important: the logically separate R/W ports in this class
// may be combined into a single, multiplexed R/W port.
class Array : public Block {
    Interface _write;
    Interface _read;

    llvm::Type* GetWriteInputType(llvm::Type* type, unsigned depth);
    llvm::Type* GetReadInputType(llvm::Type* type, unsigned depth);
    llvm::Type* GetReadOutputType(llvm::Type* type, unsigned depth);

public:
    Array(llvm::Type* type, unsigned depth);
    virtual ~Array() { }

    virtual bool hasState() const {
        return true;
    }

    DEF_GET(write);
    DEF_GET(read);
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__
