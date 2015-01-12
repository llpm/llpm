#ifndef __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__

#include <libraries/core/mem_intr.hpp>

namespace llpm {

// In RTL, a register has one write port and an output that continuously
// outputs the value which the register contains.
class RTLReg : public Block {
    llvm::Type* _type;
    Interface _write;
    std::vector<Interface*> _read;

public:
    RTLReg(llvm::Type* datatype);

    DEF_GET_NP(type);
    DEF_GET(write);
    DEF_ARRAY_GET(read);

    virtual bool hasState() const {
        return true;
    }

    Interface* newRead();

    virtual DependenceRule depRule(const OutputPort* op) const {
        if (op == _write.dout())
            return DependenceRule(DependenceRule::AND,
                                  DependenceRule::Always);
        // Assume it's the read port otherwise
        return DependenceRule(DependenceRule::Custom,
                              DependenceRule::Maybe);
    }

    virtual const std::vector<InputPort*>& deps(
            const OutputPort* op) const {
        return inputs();
    }

    virtual bool outputsSeparate() const {
        return true;
    }
};


// Block RAM (BRAM) are assumed to have N true RW ports, zero R ports
// and zero W ports.
// Port req format:
//   { isWrite : i1, data : *, idx : i* } -- for reads, data is garbage
// Port resp format:
//   { data : * } -- for writes, data is garbage
class BlockRAM : public Block {
    llvm::Type* _type;
    unsigned _depth;
    std::vector<Interface*> _ports;
    std::map<const OutputPort*, std::vector<InputPort*> > _deps;

public:
    BlockRAM(llvm::Type* ty, unsigned depth, unsigned numPorts);

    DEF_GET_NP(type);
    DEF_GET_NP(depth);
    DEF_ARRAY_GET(ports);

    virtual bool hasState() const {
        return true;
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        return DependenceRule(DependenceRule::OR,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
        deps(const OutputPort* op) const {
            return _deps.find(op)->second;
    }

    virtual bool outputsSeparate() const {
        return true;
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
