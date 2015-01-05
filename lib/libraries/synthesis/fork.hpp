#ifndef __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__

#include <llpm/block.hpp>

namespace llpm {

class Fork : public Block {
    InputPort _din;
    std::vector<OutputPort*> _dout;

public:
    Fork(llvm::Type* ty) :
        _din(this, ty, "din")
    { }

    DEF_GET(din);
    DEF_ARRAY_GET(dout);

    OutputPort* createOutput();

    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        return inputs();
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNTHESIS_FORK_HPP__
