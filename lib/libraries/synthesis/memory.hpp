#ifndef __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__

#include <libraries/core/mem_intr.hpp>

namespace llpm {

/**
 * In RTL, a register has one write port and an output that
 * continuously outputs the value which the register contains.
 */
class RTLReg : public Block {
    llvm::Type* _type;
    Interface _write;
    std::vector<std::unique_ptr<Interface>> _read;

public:
    RTLReg(llvm::Type* datatype);

    DEF_GET_NP(type);
    DEF_GET(write);
    DEF_UNIQ_ARRAY_GET(read);

    virtual bool hasState() const {
        return true;
    }

    Interface* newRead();

    virtual DependenceRule deps(const OutputPort* op) const {
        if (op == _write.dout())
            return DependenceRule(DependenceRule::AND_FireOne, {_write.din()});
        for (const auto& read: _read) {
            if (read->dout() == op)
                return DependenceRule(DependenceRule::AND_FireOne, {read->din()});
        }
        assert(false);
    }

    virtual bool outputsSeparate() const {
        return true;
    }

    virtual float logicalEffort(const InputPort* ip, const OutputPort*) const {
        if (ip == _write.din())
            return 0.5;
        return 0.1;
    }
};


/**
 * Block RAM (BRAM) are assumed to have N true RW ports, zero R ports 
 * and zero W ports. 
 *   Port req format: 
 *     { isWrite : i1, data : *, idx : i* } -- for reads, data is garbage
 *   Port resp format:
 *     { data : * } -- for writes, data is garbage
 */
class BlockRAM : public Block {
    llvm::Type* _type;
    unsigned _depth;
    std::vector<std::unique_ptr<Interface>> _ports;

public:
    BlockRAM(llvm::Type* ty, unsigned depth, unsigned numPorts);

    DEF_GET_NP(type);
    DEF_GET_NP(depth);
    DEF_UNIQ_ARRAY_GET(ports);

    virtual bool hasState() const {
        return true;
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        for (const auto& port: _ports) {
            if (op == port->dout())
                return DependenceRule(DependenceRule::AND_FireOne,
                                      {port->din()});
        }
        assert(false && "OP doesn't belong!");
    }

    virtual bool outputsSeparate() const {
        return true;
    }

    virtual float logicalEffort(const InputPort*, const OutputPort*) const {
        return 1.0;
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
