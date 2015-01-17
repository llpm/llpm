#ifndef __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__

#include <llpm/block.hpp>

namespace llpm {

class Fork : public Block {
    InputPort _din;
    // Is this a virtual fork? Virtual forks only pass through LI
    // metadata combinatorially. Whomever is synthesizing forks has to
    // ensure that the fork can validly be combinatorial.
    bool _virt;
    std::vector<OutputPort*> _dout;

public:
    Fork(llvm::Type* ty, bool virt) :
        _din(this, ty, "din"),
        _virt(virt)
    { }

    DEF_GET(din);
    DEF_GET_NP(virt);
    DEF_ARRAY_GET(dout);

    OutputPort* createOutput();

    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule depRule(const OutputPort*) const {
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual bool outputsSeparate() const {
        return true;
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort*) const {
        return inputs();
    }

    virtual std::string print() const {
        if (_virt)
            return "virt";
        return "";
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__
