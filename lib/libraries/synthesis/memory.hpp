#ifndef __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__

#include <libraries/core/mem_intr.hpp>

namespace llpm {

// In RTL, a register has one write port and an output that continuously
// outputs the value which the register contains.
class RTLReg : public Block {
    llvm::Type* _type;
    Interface _write;
    OutputPort _read;

public:
    RTLReg(llvm::Type* datatype);

    DEF_GET_NP(type);
    DEF_GET(write);
    DEF_GET(read);

    virtual bool hasState() const {
        return true;
    }

    virtual FiringRule firing() const {
        return OR;
    }

    virtual bool outputsIndependent() const {
        return true;
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
