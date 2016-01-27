#include "block.hpp"

#include <llpm/module.hpp>

using namespace std;

namespace llpm {


#if 0
bool Block::refine(std::vector<Block*>& blocks) {
    if (!this->refinable())
        return false;
    assert(false && "Not yet implemented!");
}
#endif

float Block::maxLogicalEffort(OutputPort* op) const {
    float max = 0.0;
    for (auto ip: op->deps().inputs) {
        float le = logicalEffort(ip, op);
        if (le > max)
            max = le;
    }
    return max;
}

string Block::globalName() const {
    auto m = module();
    if (m == NULL)
        return name();
    return m->design().namer().getName((Block*)this, m);
}


// Upon what conditions does a block accept inputs and execute?
// This routine only works properly when all the outputs have the same
// dependence rules.
DependenceRule::DepType Block::firing() const {
    if (outputs().size() == 0)
        return DependenceRule::Custom;

    auto output1 = outputs()[0];
    auto dr = deps(output1);
    for (auto&& output: outputs()) {
        dr = dr + deps(output);
    }

    return dr.depType;
}

// False if this block has multiple outputs and their dependences
// differ or are maybes. Otherwise, output tokens are always produced
// based on the same rules, so they are "dependent"
bool Block::outputsTied() const {
    set<const InputPort*> deps;
    bool first = true;
    for (auto output: outputs()) {
        auto dr = output->deps();
        if (first) {
            deps.insert(dr.inputs.begin(), dr.inputs.end());
            first = false;
        } else {
            set<const InputPort*> lcldeps(dr.inputs.begin(), dr.inputs.end());
            if (deps != lcldeps)
                return false;
        }
        if (output->deps().depType != 
                DependenceRule::AND_FireOne)
            return false;
    }

    return true;
}

void Block::deps(const OutputPort* op, std::set<const InputPort*>& ret) const {
    auto depsVec = deps(op).inputs;
    ret.insert(depsVec.begin(), depsVec.end());
}

void Block::deps(const InputPort* ip, std::set<const OutputPort*>& ret) const {
    for(OutputPort* op: outputs()) {
        auto depsVec = deps(op).inputs;
        if (find(depsVec.begin(), depsVec.end(), ip) != depsVec.end())
            ret.insert(op);
    }
}

} // namespace llpm
