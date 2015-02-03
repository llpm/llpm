#ifndef __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__

#include <llpm/block.hpp>
#include <llpm/design.hpp>

#include <set>

namespace llpm {

class PipelineRegister;

/**
 * This class contains only the control logic for pipeline registers.
 * It has the ability to control an arbitrary number of pipeline
 * registers, amortizing their control logic.
 */
class PipelineStageController : public Block {
    friend class PipelineRegister;

    /// The vin port is used to get a valid signal
    InputPort _vin;
    /// The vout port is used to output a valid signal
    OutputPort _vout;

    /// The ce ("clock enable") port is used to control the pipeline
    /// registers
    OutputPort _ce;

    std::set<PipelineRegister*> _regs;

    void add(PipelineRegister* reg) {
        _regs.insert(reg);
    }

    void remove(PipelineRegister* reg) {
        _regs.erase(reg);
    }

public:
    PipelineStageController(Design& d);

    DEF_GET(vin);
    DEF_GET(vout);
    DEF_GET(ce);

    void connectVin(ConnectionDB* conns, OutputPort* op);

    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule depRule(const OutputPort*) const {
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort*) const {
        return inputs();
    }
};

class PipelineRegister : public Block {
    const Port* _source;
    InputPort _din;
    InputPort* _enable;
    OutputPort _dout;

public:
    PipelineRegister(const Port* src) :
        _source(src),
        _din(this, src->type(), "d"),
        _enable(nullptr),
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
    DEF_GET_NP(enable);
    DEF_GET(dout);
    DEF_GET_NP(source);

    void controller(ConnectionDB*, PipelineStageController*);

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

