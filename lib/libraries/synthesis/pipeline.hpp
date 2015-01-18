#ifndef __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__

#include <llpm/block.hpp>

#include <set>

namespace llpm {

class PipelineRegister;

class PipelineStageController {
    friend class PipelineRegister;

    std::set<PipelineRegister*> _regs;

    void add(PipelineRegister* reg) {
        _regs.insert(reg);
    }

    void remove(PipelineRegister* reg) {
        _regs.erase(reg);
    }

public:
    PipelineStageController() { }
};

class PipelineRegister : public Block {
    OutputPort* _source;
    InputPort _din;
    OutputPort _dout;

public:
    PipelineRegister(OutputPort* src) :
        _source(src),
        _din(this, src->type(), "d"),
        _dout(this, src->type(), "q")
    {
        auto ownerName = _source->owner()->name();
        if (ownerName != "")
            this->name(ownerName + "_reg");
        this->history().setOptimization(src->ownerP());
    }

    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);
    DEF_GET_NP(source);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

class Latch: public Block {
    OutputPort* _source;
    InputPort _din;
    OutputPort _dout;

public:
    Latch(OutputPort* src) :
        _source(src),
        _din(this, src->type(), "d"),
        _dout(this, src->type(), "q")
    {
        auto ownerName = _source->owner()->name();
        if (ownerName != "")
            this->name(ownerName + "_latch");
        this->history().setOptimization(src->ownerP());
    }

    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);
    DEF_GET_NP(source);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

};

#endif // __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__

