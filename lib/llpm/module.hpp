#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/design.hpp>
#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/comm_intr.hpp>

#include <boost/foreach.hpp>

namespace llpm {

class Design;

class Module : public Block {
protected:
    Design& _design;
    std::string _name;

public:
    Module(Design& design, std::string name) :
        _design(design),
        _name(name)
    { }
    virtual ~Module() { }

    DEF_GET_NP(name);
    DEF_GET_NP(design);

    virtual bool hasState() const = 0;
    virtual bool isPure() const {
        return !hasState();
    }

    virtual void blocks(vector<Block*>&) const = 0;
    virtual bool internalRefine(Design::Refinery::StopCondition* sc = NULL) = 0;
};

/**********
 * This module simply contains blocks. It is likely to be the most
 * common module class and a good default module base class. Should
 * be sub-classed to define input and output ports.
 */
class ContainerModule : public Module {
    // The blocks which I directly contain
    set<Block*> _blocks;

    // My submodules
    set<Module*> _modules;

    // The connections between blocks in this module
    ConnectionDB _conns;

    // Each container module input and output port have a pass
    // through block and a mapping to the internal output and input
    // port on the corresponding pass through block.
    map<InputPort*, Identity> _inputMap;
    map<OutputPort*, Identity> _outputMap;

protected:
    void addInputPort(InputPort* ip);
    void addOutputPort(OutputPort* op);

public:
    ContainerModule(Design& design, std::string name) :
        Module(design, name),
        _conns(this)
    { }

    const OutputPort* getDriver(InputPort* ip) const {
        auto f = _inputMap.find(ip);
        assert(f != _inputMap.end());
        const OutputPort* internalIPDriver = f->second.dout();
        return internalIPDriver;
    }

    const InputPort* getSink(OutputPort* op) const {
        auto f = _outputMap.find(op);
        assert(f != _outputMap.end());
        const InputPort* internalOPSink = f->second.din();
        return internalOPSink;
    }

    void addBlock(Block* b) {
        assert(b != NULL);
        if (Module* m = dynamic_cast<Module*>(b)) {
            addSubmodule(m);
        } else {
            _blocks.insert(b);
            b->module(this);
        }
    }

    void addSubmodule(Module* m) {
        assert(m != NULL);
        this->_modules.insert(m);
        m->module(this);
    }

    void connect(const InputPort* sink, const OutputPort* source) {
        connect(source, sink);
    }
    void connect(const OutputPort* source, const InputPort* sink) {
        if (source == NULL || sink == NULL)
            throw InvalidArgument("Neither source nor sink can be NULL!");

        Block* sourceB = source->owner();
        Block* sinkB   = sink->owner();

        if (sourceB->module() != NULL && sourceB->module() != this) {
            throw InvalidArgument("Source block is not contained by this module!");
        }

        if (sinkB->module() != NULL && sinkB->module() != this) {
            throw InvalidArgument("Sink block is not contained by this module!");
        }

        if (sourceB->module() == NULL) {
            sourceB->module(this);
        }

        if (sinkB->module() == NULL) {
            sinkB->module(this);
        }
    }

    virtual bool hasState() const {
        BOOST_FOREACH(Block* b, _blocks) {
            if (b->hasState())
                return true;
        }

        BOOST_FOREACH(Module* m, _modules) {
            if (m->hasState())
                return true;
        }

        return false;
    }

    virtual void blocks(vector<Block*>& vec) const {
        vec.insert(vec.end(), _blocks.begin(),  _blocks.end());
        vec.insert(vec.end(), _modules.begin(), _modules.end());
    }
    
    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(std::vector<Block*>& blocks,
                        std::map<InputPort*, vector<InputPort*> >& ipMap,
                        std::map<OutputPort*, OutputPort*>& opMap) const;
    
    virtual bool internalRefine(Design::Refinery::StopCondition* sc = NULL);
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
