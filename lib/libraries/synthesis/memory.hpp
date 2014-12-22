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

    virtual DependenceRule depRule(const OutputPort* op) const {
        if (op == _write.dout())
            return DependenceRule(DependenceRule::AND,
                                  DependenceRule::Always);
        else if (op == &_read)
            return DependenceRule(DependenceRule::Custom,
                                  DependenceRule::Maybe);
        else
            assert(false);
    }
    virtual const std::vector<InputPort*>& deps(const OutputPort* op) const {
        assert(op == _write.dout() ||
               op == &_read);
        return inputs();
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_MEMORY_HPP__
