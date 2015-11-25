#ifndef __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__
#define __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>
#include <libraries/core/interface.hpp>

namespace llpm {

// Basic abstraction for storage. Could be a register, block RAM,
// main memory, ect. In this basic abstraction and all memories have one
// read and one write port.  Further technology-dependent transformations
// will change the memory to something more realistic and whatever
// arbitration may be necessary.
class Memory : public Block {
    llvm::Type* _type;
    Interface _write;
    Interface _read;
    std::vector<InputPort*> _writeDeps;
    std::vector<InputPort*> _readDeps;

    llvm::Type* GetWriteReq(llvm::Type* dt, llvm::Type* idx);
    llvm::Type* GetReadReq(llvm::Type* dt, llvm::Type* idx);
protected:
    Memory(llvm::Type* datatype,
           llvm::Type* idxType);

public:
    DEF_GET_NP(type);
    DEF_GET(write);
    DEF_GET(read);

    virtual bool hasState() const {
        return true;
    }

    virtual DependenceRule deps(const OutputPort* op) const {
        if (op == _write.dout())
            return DependenceRule(DependenceRule::AND_FireOne, {_write.din()});
        else if (op == _read.dout())
            return DependenceRule(DependenceRule::AND_FireOne, {_read.din()});
        assert(false && "OP don't belong!");
    }
};

// Accepts an input on the write interface and stores it. Outputs the
// stored input when a request arrives on a read interface. Has separate
// read/write ports so that reads are not serialized with writes. Later
// optimizations are likely to create multiple read ports if an
// InterfaceMultiplexer is connected to the read port, depending on the
// target technology.
class Register : public Memory {
public:
    Register(llvm::Type* type);
    virtual ~Register() { }
};

// RAM. Has one write port and one read port which can be multiplexed.
// Later technology-specific optimizations can instantiate RAMs with
// different R/W port configurations and optimize the connectivity.
// Potentially important: the logically separate R/W ports in this class
// may be combined into a single, multiplexed R/W port.
class FiniteArray : public Memory {
    unsigned _depth;
public:
    FiniteArray(llvm::Type* type, unsigned depth);
    virtual ~FiniteArray() { }

    DEF_GET_NP(depth);

    float logicalEffort(const InputPort*, const OutputPort*) const {
        return 10.0;
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_MEM_INTR_HPP__
