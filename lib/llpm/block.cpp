#include "block.hpp"

namespace llpm {


#if 0
bool Block::refine(std::vector<Block*>& blocks) {
    if (!this->refinable())
        return false;
    assert(false && "Not yet implemented!");
}
#endif


// Upon what conditions does a block accept inputs and execute?
// This routine only works properly when all the outputs have the same
// dependence rules.
DependenceRule::InputType Block::firing() const {
    if (outputs().size() == 0)
        return DependenceRule::OR;

    auto output1 = outputs()[0];
    const std::vector<InputPort*>& canon = deps(output1);
    const DependenceRule drCanon = depRule(output1);
    for (auto&& output: outputs()) {
        auto dr = depRule(output);
        auto odeps = deps(output);
        if (dr != drCanon || odeps != canon)
            return DependenceRule::Custom;
    }

    return drCanon.inputType();
}

// False if this block has multiple outputs and their dependences
// differ or are maybes. Otherwise, output tokens are always produced
// based on the same rules, so they are "dependent"
bool Block::outputsIndependent() const {
    if (outputs().size() <= 1)
        return false;

    auto output1 = outputs()[0];
    const std::vector<InputPort*>& canon = deps(output1);
    const DependenceRule drCanon = depRule(output1);
    for (auto&& output: outputs()) {
        auto dr = depRule(output);
        auto odeps = deps(output);
        if (dr != drCanon || odeps != canon ||
            dr.outputType() != DependenceRule::Always) 
            return true;
    }

    return false;
}

void Block::deps(const OutputPort* op, std::set<InputPort*>& ret) const {
    auto depsVec = deps(op);
    ret.insert(depsVec.begin(), depsVec.end());
}

void Block::deps(const InputPort* ip, std::set<OutputPort*>& ret) const {
    for(OutputPort* op: outputs()) {
        auto depsVec = deps(op);
        if (find(depsVec.begin(), depsVec.end(), ip) != depsVec.end())
            ret.insert(op);
    }
}

} // namespace llpm
