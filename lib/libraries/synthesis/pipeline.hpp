#ifndef __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__
#define __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__

#include <llpm/block.hpp>

#include <set>

namespace llpm {

class PipelineRegister;

class PipelineStageController : public gc_cleanup {
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
    PipelineStageController* _controller;
    OutputPort* _source;
    InputPort _din;
    OutputPort _dout;

public:
    PipelineRegister(OutputPort* src,
                     PipelineStageController* controller = NULL) :
        _controller(NULL),
        _source(src),
        _din(this, src->type(), "d"),
        _dout(this, src->type(), "q")
    {
        this->controller(controller);
        auto ownerName = _source->owner()->name();
        if (ownerName != "")
            this->name(ownerName + "_reg");
        this->history().setOptimization(src->owner());
    }

    void controller(PipelineStageController* controller) {
        if (this->_controller != NULL)
            _controller->remove(this);

        this->_controller = controller;
        if (_controller != NULL)
            _controller->add(this);
    }

    virtual bool hasState() const {
        return true;
    }

    DEF_GET_NP(controller);
    DEF_GET(din);
    DEF_GET(dout);
    DEF_GET_NP(source);
};

};

#endif // __LLPM_LIBRARIES_SYNTHESIS_PIPELINE_HPP__

