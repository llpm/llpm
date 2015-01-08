#include "module.hpp"

#include <synthesis/pipeline.hpp>
#include <analysis/graph_queries.hpp>

#include <boost/format.hpp>

#include <deque>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace std;

namespace llpm {

void Module::internalDrivers(std::vector<OutputPort*>& drivers) const {
    for (auto input: inputs()) {
        drivers.push_back(this->getDriver(input));
    }
}

ContainerModule::~ContainerModule() {
    if (_pipeline)
        delete _pipeline;
}

InputPort* ContainerModule::addInputPort(InputPort* ip, std::string name) {
    if (name == "") {
        set<string> ipNames;
        for (auto currIP: inputs())
            ipNames.insert(currIP->name());
        unsigned num = 0;
        while (true) {
            name = str(boost::format("input%1%") % num);
            if (ipNames.count(name) == 0)
                break;
            num++;
        }
    }
    auto dummy = new DummyBlock(ip->type());
    InputPort* extIp = new InputPort(this, ip->type(), name);
    _inputMap.insert(make_pair(extIp, dummy));
    dummy->name(name + "_dummy");
    conns()->blacklist(dummy);
    this->conns()->connect(dummy->dout(), ip);
    return extIp;
}

void ContainerModule::removeInputPort(InputPort* ip) {
    auto f = _inputMap.find(ip);
    if (f == _inputMap.end())
        throw InvalidArgument("Cannot remove InputPort which doesn't seem to exist!");
    _inputMap.erase(f);
    auto vecLoc = find(_inputs.begin(), _inputs.end(), ip);
    assert(vecLoc != _inputs.end());
    _inputs.erase(vecLoc);
    delete ip;
}

OutputPort* ContainerModule::addOutputPort(OutputPort* op, std::string name) {
    if (name == "") {
        set<string> opNames;
        for (auto currOP: outputs())
            opNames.insert(currOP->name());
        unsigned num = 0;
        while (true) {
            name = str(boost::format("output%1%") % num);
            if (opNames.count(name) == 0)
                break;
            num++;
        }
    }

    auto dummy = new DummyBlock(op->type());
    OutputPort* extOp = new OutputPort(this, op->type(), name);
    _outputMap.insert(make_pair(extOp, dummy));
    dummy->name(name + "_dummy");
    conns()->blacklist(dummy);
    this->conns()->connect(op, dummy->din());
    return extOp;
}

void ContainerModule::removeOutputPort(OutputPort* op) {
    InputPort* internalSink = getSink(op);
    OutputPort* internalDriver = _conns.findSource(internalSink);
    if (internalDriver)
        _conns.disconnect(internalDriver, internalSink);
    auto f = _outputMap.find(op);
    if (f == _outputMap.end())
        throw InvalidArgument("Cannot remove OutputPort which doesn't seem to exist!");
    _outputMap.erase(f);
    auto vecLoc = find(_outputs.begin(), _outputs.end(), op);
    _outputs.erase(vecLoc);
    delete op;
}

Interface* ContainerModule::addClientInterface(
        OutputPort* req, InputPort* resp, std::string name) {
    auto iface = new Interface(this, resp->type(), req->type(), false, name);

    auto opdummy = new DummyBlock(req->type());
    _outputMap.insert(make_pair(iface->dout(), opdummy));
    opdummy->name(name + "_opdummy");
    conns()->blacklist(opdummy);
    conns()->connect(req, opdummy->din());

    auto ipdummy = new DummyBlock(resp->type());
    _inputMap.insert(make_pair(iface->din(), ipdummy));
    ipdummy->name(name + "_ipdummy");
    conns()->blacklist(ipdummy);
    conns()->connect(resp, ipdummy->dout());

    return iface;
}

Interface* ContainerModule::addServerInterface(
        InputPort* req, OutputPort* resp, std::string name) {
    auto iface = new Interface(this, req->type(), resp->type(), true, name);

    auto opdummy = new DummyBlock(resp->type());
    _outputMap.insert(make_pair(iface->dout(), opdummy));
    opdummy->name(name + "_opdummy");
    conns()->blacklist(opdummy);
    conns()->connect(resp, opdummy->din());

    auto ipdummy = new DummyBlock(req->type());
    _inputMap.insert(make_pair(iface->din(), ipdummy));
    ipdummy->name(name + "_ipdummy");
    conns()->blacklist(ipdummy);
    conns()->connect(req, ipdummy->dout());

    return iface;
}


bool ContainerModule::pipelineable(const OutputPort* op) const {
    auto intSink = getSink(op);
    auto intSource = _conns.findSource(intSink);
    if (intSource == NULL)
        return true;
    return intSource->pipelineable();
}

bool ContainerModule::refine(ConnectionDB& conns) const
{
    conns.update(_conns);

    for(InputPort* ip: _inputs) {
        auto f = _inputMap.find(ip);
        assert(f != _inputMap.end());
        conns.remap(ip, f->second->din());
        conns.deblacklist(getDriver(ip)->owner());
    }

    for(OutputPort* op: _outputs) {
        auto f = _outputMap.find(op);
        assert(f != _outputMap.end());
        conns.remap(op, f->second->dout());
        conns.deblacklist(getSink(op)->owner());
    }

    return true;
}

unsigned ContainerModule::internalRefine(int depth,
                                         Refinery::StopCondition* sc) {
    vector<Block*> blocksTmp;
    blocks(blocksTmp);

    auto passes = design().refinery().refine(blocksTmp, _conns, depth, sc);
    return passes;
}

Pipeline* ContainerModule::pipeline() {
    if (_pipeline == NULL) {
        _pipeline = new Pipeline(this);
    }
    return _pipeline;
}

void ContainerModule::validityCheck() const {
    set<string> portNames;
    for (InputPort* ip: _inputs) {
        assert(_inputMap.count(ip) == 1);
        assert(portNames.count(ip->name()) == 0);
        portNames.insert(ip->name());
    }

    for (OutputPort* op: _outputs) {
        assert(_outputMap.count(op) == 1);
        assert(portNames.count(op->name()) == 0);
        portNames.insert(op->name());
    }

    set<Block*> blocks;
    _conns.findAllBlocks(blocks);
    for (Block* b: blocks) {
        assert(b->module() == this);
    }

    vector<Module*> sm;
    this->submodules(sm);
    for (Module* m: sm) {
        m->validityCheck();
    }

}

bool ContainerModule::hasCycle() const {
    vector<OutputPort*> init;
    for (auto p: _inputMap) {
        init.push_back(p.second->dout());
    }
    return queries::BlockCycleExists(&_conns, init);
}

DependenceRule ContainerModule::depRule(const OutputPort* op) const {
    //TODO: Do a graph search to determine this precisely
    return DependenceRule(DependenceRule::Custom,
                          DependenceRule::Maybe);
}

const std::vector<InputPort*>& ContainerModule::deps(
    const OutputPort*) const {
    //TODO: Do a graph search to determine this precisely
    return inputs();
}

void Module::createSWModule(llvm::Module* M) {
    // Copy the module and everything in it, _except_ for function
    // bodies. Unfortunately, LLVM's CloneModule copies the function
    // bodies, so we'll copy their code and remove that copy part.
    if (this->_swModule == NULL) {
        // Create interface type for myself
        _ifaceType = llvm::StructType::create(
            _design.context(), "class." + name());
        
        this->_swModule.reset(
            CloneModule(M, VMap));
    }
}

void Module::swModule(llvm::Module* mod) {
    set<llvm::Function*> newTests;
    for (llvm::Function* test: _tests) {
        auto newTest = mod->getFunction(test->getName());
        if (newTest == NULL)
            throw InvalidArgument(
                "Can only replace module with another when all referenced "
                "functions exist in both!");
        newTests.insert(newTest);
    }
    _tests.swap(newTests);


    std::map<std::string, llvm::Function*> newSWVersion;
    for (auto p: _swVersion) {
        auto newFunc = mod->getFunction(p.second->getName());
        if (newFunc == NULL)
            throw InvalidArgument(
                "Can only replace module with another when all referenced "
                "functions exist in both!");
        newSWVersion[p.first] = newFunc;
    }
    _swVersion.swap(newSWVersion);

    _swModule.reset(mod);
}

llvm::Function* Module::cloneFunc(
    llvm::Function* I, std::string name) {
    llvm::Function* existing = swModule()->getFunction(I->getName());
    if (existing != NULL) {
        existing->setName(name);
        return existing;
    } 

    llvm::Function* NF = llvm::Function::Create(
            llvm::cast<llvm::FunctionType>(
                I->getType()->getElementType()),
            I->getLinkage(), name, swModule());
    NF->copyAttributesFrom(I);
    VMap[I] = NF;

    llvm::Function *F = llvm::cast<llvm::Function>(VMap[I]);
    if (!I->isDeclaration()) {
        llvm::Function::arg_iterator DestI = F->arg_begin();
        for (llvm::Function::const_arg_iterator J = I->arg_begin();
                J != I->arg_end(); ++J) {
            DestI->setName(J->getName());
            VMap[J] = DestI++;
        }
        llvm::SmallVector<llvm::ReturnInst*, 8> Returns;
        llvm::CloneFunctionInto(F, I, VMap,
                                /*ModuleLevelChanges=*/true, Returns);
    }
    F->setName(name);
    return F;
}

llvm::Function* Module::createCallStub(Interface* iface) {
    vector<llvm::Type*> args;
    args.push_back(llvm::PointerType::getUnqual(_ifaceType));
    llvm::Type* argType = iface->req()->type();
    if (argType->getTypeID() == llvm::Type::StructTyID) {
        for (unsigned i=0; i<argType->getStructNumElements(); i++) {
            args.push_back(argType->getStructElementType(i));
        }
    } else {
        if (!argType->isVoidTy())
            args.push_back(argType);
    }

    llvm::Type* retType = iface->resp()->type();

    llvm::FunctionType* ft = llvm::FunctionType::get(retType, args, false);
    llvm::Function* func = llvm::Function::Create(
        ft, llvm::GlobalValue::ExternalLinkage, iface->name(), swModule());
    _interfaceStubs[iface->name()] = func;
    return func;
}

} // namespace llpm
