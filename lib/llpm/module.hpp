#ifndef __LLPM_MODULE_HPP__
#define __LLPM_MODULE_HPP__

#include <llpm/design.hpp>
#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/interface.hpp>
#include <util/cache.hpp>

#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <boost/foreach.hpp>

namespace llpm {

class Design;

class Module : public Block {
protected:
    Design& _design;

    llvm::ValueToValueMapTy VMap;
    std::unique_ptr<llvm::Module> _swModule;

    // A type which represents this module in the S/W version of this
    // module
    llvm::StructType* _swType;
    // A type which represents this module's S/W interface. Typically
    // opaque
    llvm::StructType* _ifaceType;

    llvm::Function* cloneFunc(llvm::Function* F, std::string name);
    llvm::Function* createCallStub(Interface*);
    llvm::Function* createPortStub(Port*);

    std::map<std::string, llvm::Function*> _interfaceStubs;
    std::map<Port*, llvm::Function*> _portStubs;

    // Unit testing functions
    std::set<llvm::Function*> _tests;
    std::map<std::string, llvm::Function*> _swVersion;

public:
    Module(Design& design, std::string name) :
        _design(design)
    {
        this->name(name);
    }
    virtual ~Module() { }

    DEF_GET_NP(design);
    llvm::Module* swModule() {
        return _swModule.get();
    }
    virtual void swModule(llvm::Module* mod);
    void createSWModule(llvm::Module*);
    DEF_GET_NP(swType);
    DEF_GET_NP(ifaceType);

    llvm::Function* swVersion(std::string name) {
        auto f = _swVersion.find(name);
        if (f != _swVersion.end())
            return f->second;
        return NULL;
    }
    std::set<llvm::Function*> tests() {
        return _tests;
    }
    llvm::Function* interfaceStub(std::string name) {
        auto f = _interfaceStubs.find(name);
        if (f == _interfaceStubs.end())
            return NULL;
        return f->second;
    }
    const std::map<std::string, llvm::Function*>& interfaceStubs() {
        return _interfaceStubs;
    }

    virtual bool hasState() const = 0;
    virtual bool isPure() const {
        return !hasState();
    }

    // For opaque modules, this may return NULL
    virtual ConnectionDB* conns() = 0;
    // Return the current change count -- useful for tracking if changes have
    // been made to a module
    virtual uint64_t changeCounter() = 0;

    virtual void blocks(std::vector<Block*>&) const = 0;
    virtual void submodules(std::vector<Module*>&) const = 0;
    virtual void submodules(std::vector<Block*>& vec) const = 0;

    virtual unsigned internalRefine(
        int depth = -1,
        Refinery::StopCondition* sc = NULL) = 0;

    virtual bool refined(Refinery::StopCondition* sc) {
        std::vector<Block*> crude;
        this->blocks(crude);
        return sc->refined(crude);
    }
    virtual void validityCheck() const = 0;

    virtual OutputPort* getDriver(const InputPort* ip) const = 0;
    virtual InputPort* getSink(const OutputPort* op) const = 0;
    void internalDrivers(std::vector<OutputPort*>&) const;

    virtual bool hasCycle() const = 0;
};

class DummyBlock: public Identity {
    Port* _modPort;
public:
    DummyBlock(llvm::Type* t, Port* modPort) :
        Identity(t),
        _modPort(modPort) { }

    DummyBlock(Port* modPort) :
        Identity(modPort->type()),
        _modPort(modPort) { }
    
    DEF_GET_NP(modPort);
};

// A module which a third party can edit
class MutableModule: public Module {
public:
    MutableModule(Design& design, std::string name) :
        Module(design, name) { }

    virtual ConnectionDB* conns() = 0;
    virtual const ConnectionDB* conns() const = 0;

    virtual uint64_t changeCounter() {
        return conns()->changeCounter();
    }

    virtual bool hasState() const {
        std::set<Block*> blocks;
        conns()->findAllBlocks(blocks);
        for(Block* b: blocks) {
            if (b->hasState())
                return true;
        }

        return false;
    }

    virtual void blocks(std::vector<Block*>& vec) const {
        std::set<Block*> blocks;
        conns()->findAllBlocks(blocks);
        vec.insert(vec.end(), blocks.begin(), blocks.end());
    }

    virtual void blocks(std::set<Block*>& blocks) const {
        conns()->findAllBlocks(blocks);
    }

    virtual unsigned size() const {
        std::set<Block*> b;
        blocks(b);
        return b.size();
    }

    virtual void submodules(std::vector<Block*>& vec) const {
        std::set<Block*> blocks;
        conns()->findAllBlocks(blocks);
        for (Block* b: blocks) {
            Module* m = dynamic_cast<Module*>(b);
            if (m != NULL)
                vec.push_back(m);
        }
    }

    virtual void submodules(std::vector<Module*>& vec) const {
        std::set<Block*> blocks;
        conns()->findAllBlocks(blocks);
        for (Block* b: blocks) {
            Module* m = dynamic_cast<Module*>(b);
            if (m != NULL)
                vec.push_back(m);
        }
    }
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
    std::map<InputPort*, DummyBlock*> _inputMap;
    std::map<OutputPort*, DummyBlock*> _outputMap;

    InputPort* addInputPort(InputPort* ip, std::string name = "");
    void removeInputPort(InputPort* ip);
    OutputPort* addOutputPort(OutputPort* op, std::string name = "");
    void removeOutputPort(OutputPort* ip);
    Interface* addClientInterface(OutputPort* req,
                                  InputPort* resp,
                                  std::string name = "");
    Interface* addServerInterface(InputPort* req,
                                  OutputPort* resp,
                                  std::string name = "");
    void removeInterface(Interface* iface);

    /***
     * Cached data
     */
    Cache<bool> _hasCycle;
    bool _hasCycleCompute() const;

    struct Deps {
        DependenceRule rule;
        std::vector<InputPort*> deps;
    };
    CacheMap<const OutputPort*, Deps> _deps;
    Deps _findDeps(const OutputPort*);

public:
    ContainerModule(Design& design, std::string name) :
        MutableModule(design, name),
        _conns(this),
        _hasCycle(_conns.counterPtr(),
                  boost::bind(&ContainerModule::_hasCycleCompute, this)),
        _deps(_conns.counterPtr(),
              boost::bind(&ContainerModule::_findDeps, this, _1))
    { }

    virtual ~ContainerModule();

    ConnectionDB* conns() {
        return &_conns;
    }
    const ConnectionDB* conns() const {
        return &_conns;
    } 

    virtual void validityCheck() const;

    OutputPort* getDriver(const InputPort* ip) const {
        // Strip away const-ness for lookup. Just don't touch it
        auto f = _inputMap.find((InputPort*)ip);
        assert(f != _inputMap.end());
        OutputPort* internalIPDriver = f->second->dout();
        return internalIPDriver;
    }

    InputPort* getSink(const OutputPort* op) const {
        // Strip away const-ness for lookup. Just don't touch it
        auto f = _outputMap.find((OutputPort*)op);
        assert(f != _outputMap.end());
        InputPort* internalOPSink = f->second->din();
        return internalOPSink;
    }

    virtual bool pipelineable(const OutputPort*) const;

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


    
    virtual bool refine(ConnectionDB& conns) const;
    virtual bool refinable() const {
        return true;
    }

    virtual bool hasCycle() const {
        return this->_hasCycle();
    }

    
    virtual unsigned internalRefine(int depth = -1,
                                    Refinery::StopCondition* sc = NULL);

    virtual bool outputsSeparate() const;
    virtual DependenceRule depRule(const OutputPort* op) const;
    virtual const std::vector<InputPort*>& deps(const OutputPort*) const;
};

} // namespace llpm

#endif // __LLPM_MODULE_HPP__
