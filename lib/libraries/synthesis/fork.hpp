#pragma once

#include <llpm/block.hpp>
#include <cmath>

namespace llpm {

class Fork : public Block {
    InputPort _din;
    // Is this a virtual fork? Virtual forks only pass through LI
    // metadata combinatorially. Whomever is synthesizing forks has to
    // ensure that the fork can validly be combinatorial.
    bool _virt;
    std::vector<std::unique_ptr<OutputPort>> _dout;

public:
    Fork(llvm::Type* ty, bool virt) :
        _din(this, ty, "din"),
        _virt(virt)
    { }

    DEF_GET(din);
    DEF_GET_NP(virt);
    DEF_UNIQ_ARRAY_GET(dout);

    OutputPort* createOutput();

    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule deps(const OutputPort*) const {
        return DependenceRule(DependenceRule::AND_FireOne, inputs());
    }

    virtual bool outputsSeparate() const {
        return true;
    }

    virtual std::string print() const {
        if (_virt)
            return "virt";
        return "";
    }

    float logicalEffort(const InputPort*, const OutputPort*) const {
        if (_virt)
            return 0.0;
        return std::log2((float)_dout.size());
    }
};

} // namespace llpm

