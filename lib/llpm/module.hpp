#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/design.hpp>
#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <libraries/core/comm_intr.hpp>

#include <boost/foreach.hpp>

namespace llpm {

class Design;
class Schedule;
class Pipeline;

class Module : public Block {
protected:
    Design& _design;

public:
    Module(Design& design, std::string name) :
        _design(design)
    {
        this->name(name);
    }
    virtual ~Module() { }

    DEF_GET_NP(design);

    virtual bool hasState() const = 0;
    virtual bool isPure() const {
        return !hasState();
    }

    // For opaque modules, this may return NULL
    virtual ConnectionDB* conns() = 0;

    virtual void blocks(vector<Block*>&) const = 0;
    virtual void submodules(vector<Module*>&) const = 0;
    virtual void submodules(vector<Block*>& vec) const = 0;

    virtual unsigned internalRefine(Refinery::StopCondition* sc = NULL) = 0;
    virtual bool refined(Refinery::StopCondition* sc) {
        vector<Block*> crude;
        this->blocks(crude);
        return sc->refined(crude);
    }
    virtual Schedule* schedule() = 0;
    virtual Pipeline* pipeline() = 0;
    virtual void validityCheck() const = 0;

    virtual OutputPort* getDriver(InputPort* ip) const = 0;
    virtual InputPort* getSink(OutputPort* op) const = 0;

    // Modules are assumed to have independent inputs
    virtual FiringRule firing() const {
        return OR;
    }

    virtual bool hasCycle() const = 0;

    // Modules are assume to have independent outputs
    virtual bool outputsIndependent() const {
        return true;
    }
};

class DummyBlock: public Identity {
public:
    DummyBlock(llvm::Type* t) :
        Identity(t) { }
};

// A module which a third party can edit
class MutableModule: public Module {
public:
    MutableModule(Design& design, std::string name) :
        Module(design, name) { }

    virtual ConnectionDB* conns() = 0;
};

/**********
 * This module simply contains blocks. It is likely to be the most
 * common module class and a good default module base class. Should
 * be sub-classed to define input and output ports.
 */
class ContainerModule : public MutableModule {
protected:
    // The connections between blocks in this module
    ConnectionDB _conns;

    // Each container module input and output port have a pass
    // through block and a mapping to the internal output and input
    // port on the corresponding pass through block.
    map<InputPort*, DummyBlock*> _inputMap;
    map<OutputPort*, DummyBlock*> _outputMap;

    // Schedule
    Schedule* _schedule;
    Pipeline* _pipeline;

    InputPort* addInputPort(InputPort* ip, std::string name = "");
    void removeInputPort(InputPort* ip);
    OutputPort* addOutputPort(OutputPort* op, std::string name = "");
    void removeOutputPort(OutputPort* ip);

public:
    ContainerModule(Design& design, std::string name) :
        MutableModule(design, name),
        _conns(this),
        _schedule(NULL)
    { }

    virtual ~ContainerModule();

    ConnectionDB* conns() {
        return &_conns;
    }

    virtual void validityCheck() const;

    OutputPort* getDriver(InputPort* ip) const {
        auto f = _inputMap.find(ip);
        assert(f != _inputMap.end());
        OutputPort* internalIPDriver = f->second->dout();
        return internalIPDriver;
    }

    InputPort* getSink(OutputPort* op) const {
        auto f = _outputMap.find(op);
        assert(f != _outputMap.end());
        InputPort* internalOPSink = f->second->din();
        return internalOPSink;
    }

#if 0
    void addBlock(Block* b) {
        assert(b != NULL);

        Module* m = b->module();
        if (m != NULL && m != this)
            throw InvalidArgument("Cannot add already owned block to module!");

        if (Module* m = dynamic_cast<Module*>(b)) {
            addSubmodule(m);
        } else {
            _blocks.insert(b);
            b->module(this);
        }
    }
#endif

#if 0
    void addSubmodule(Module* m) {
        assert(m != NULL);
        this->_modules.insert(m);
        m->module(this);
    }
#endif

    void connect(InputPort* sink, OutputPort* source) {
        connect(source, sink);
    }
    void connect(OutputPort* source, InputPort* sink) {
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

        _conns.connect(source, sink);
    }

    virtual bool hasState() const {
        std::set<Block*> blocks;
        _conns.findAllBlocks(blocks);
        for(Block* b: blocks) {
            if (b->hasState())
                return true;
        }

        return false;
    }

    virtual void blocks(vector<Block*>& vec) const {
        std::set<Block*> blocks;
        _conns.findAllBlocks(blocks);
        vec.insert(vec.end(), blocks.begin(), blocks.end());
    }

    virtual void submodules(vector<Block*>& vec) const {
        std::set<Block*> blocks;
        _conns.findAllBlocks(blocks);
        for (Block* b: blocks) {
            Module* m = dynamic_cast<Module*>(b);
            if (m != NULL)
                vec.push_back(m);
        }
    }

    virtual void submodules(vector<Module*>& vec) const {
        std::set<Block*> blocks;
        _conns.findAllBlocks(blocks);
        for (Block* b: blocks) {
            Module* m = dynamic_cast<Module*>(b);
            if (m != NULL)
                vec.push_back(m);
        }
    }
    
    virtual bool refine(ConnectionDB& conns) const;
    virtual bool refinable() const {
        return true;
    }

    virtual bool hasCycle() const;

    
    virtual unsigned internalRefine(Refinery::StopCondition* sc = NULL);

    virtual Schedule* schedule();
    virtual Pipeline* pipeline();
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
