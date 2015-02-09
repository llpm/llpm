#include "objects.hpp"

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <libraries/core/std_library.hpp>
#include <frontends/cpphdl/library.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/ValueMap.h>


using namespace std;

namespace llpm {
namespace cpphdl {

CPPHDLClass::CPPHDLClass(Design& design,
                         std::string name,
                         llvm::StructType* ty,
                         llvm::Module* mod) :
    ContainerModule(design, name),
    _type(ty) {

    createSWModule(mod);
    _swType = swModule()->getTypeByName(ty->getName());
    _swType->setName(ty->getName().str() + "_sw");
    buildVariables();
}

void CPPHDLClass::addMember(std::string name, llvm::Function* llvmFunc, 
                            LLVMFunction* func) {
    auto funcReq = func->call()->din();
    auto join = new Join(funcReq->type());
    assert(join->din_size() > 0);
    auto nullRef = new Constant(join->din(0)->type());
    connect(nullRef->dout(), join->din(0));

    InputPort* req;
    if (join->din_size() > 1) {
        connect(join->dout(), funcReq);
        vector<llvm::Type*> argTypes;
        for (unsigned i=1; i<join->din_size(); i++) {
            argTypes.push_back(join->din(i)->type());
        } 

        auto split = new Split(argTypes);
        for (unsigned i=0; i<split->dout_size(); i++) {
            connect(split->dout(i), join->din(i+1));
        }

        req = split->din();
    } else {
        // Function has no args
        auto wait = new Wait(join->dout()->type());
        connect(join->dout(), wait->din());
        connect(wait->dout(), funcReq);
        req = wait->newControl(
            llvm::Type::getVoidTy(funcReq->type()->getContext()));
    }

    auto iface = addServerInterface(req, func->call()->dout(), name);
    connectMem(func);
    _methods[name] = func;
    createCallStub(iface);
    adoptSWVersion(name, llvmFunc);
}


void CPPHDLClass::buildVariables() {
    for (unsigned i=0; i<_type->getNumElements(); i++) {
        _variables.push_back(buildVariable(i));
    }
}

Block* CPPHDLClass::buildVariable(unsigned i) {
    assert(i < _type->getNumElements());
    llvm::Type* ty = _type->getElementType(i);

    if (ty->isPointerTy()) {
        // Build a link to an external module
        assert(false && "Not supported yet");
    } else if (ty->isSingleValueType() && !ty->isPointerTy()) {
        // Build a register
        Register* reg = new Register(ty);
        printf("Building reg for member %u: %s\n",
               i, typestr(ty).c_str());
        _regs[i] = reg;
        auto writePort = new InterfaceMultiplexer(reg->write());
        conns()->connect(reg->write(), writePort->client());
        _writePorts[i] = writePort;
        auto readPort = new InterfaceMultiplexer(reg->read());
        conns()->connect(reg->read(), readPort->client());
        _readPorts[i] = readPort;
        return reg;
    } else if (ty->isArrayTy()) {
        // Build a RAM
        llvm::ArrayType* arrTy = llvm::dyn_cast<llvm::ArrayType>(ty);
        FiniteArray* array = new FiniteArray(arrTy->getElementType(),
                                             arrTy->getNumElements());
        auto writeIM = new InterfaceMultiplexer(array->write());
        _writePorts[i] = writeIM;
        conns()->connect(writeIM->client(), array->write());
        auto readIM = new InterfaceMultiplexer(array->read());
        conns()->connect(readIM->client(), array->read());
        _readPorts[i] = readIM;
        _arrays[i] = array;
        return array;
    } else {
        throw InvalidArgument("Cannot determine how to implement type: " +
                              typestr(ty));
    }
}

void CPPHDLClass::connectMem(LLVMFunction* func) {
    auto mem = func->mem();
    for (const auto& p: mem) {
        Interface* client = p.second;
        llvm::Instruction* ins = llvm::dyn_cast<llvm::Instruction>(p.first);

        llvm::LoadInst* load = llvm::dyn_cast_or_null<llvm::LoadInst>(ins);
        llvm::StoreInst* store = llvm::dyn_cast_or_null<llvm::StoreInst>(ins);
        if (load != NULL) {
            auto ptr = load->getPointerOperand();
            unsigned varIdx = resolveMember(ptr);

            auto regF = _regs.find(varIdx);
            auto arrayF = _arrays.find(varIdx);
            auto modF = _modules.find(varIdx);
            assert(modF == _modules.end());
            assert((regF == _regs.end()) != (arrayF == _arrays.end())); 

            auto im = _readPorts[varIdx];
            assert(im != NULL);
            auto iface = im->createServer();

            if (regF != _regs.end()) {
                auto w = new Wait(iface->reqType());
                assert(iface->req() == iface->din());
                auto c = Constant::getVoid(design());
                conns()->connect(c->dout(), w->din());
                conns()->connect(w->dout(), iface->din());
                conns()->connect(client->dout(),
                                 w->newControl(client->dout()->type()));
                conns()->connect(iface->dout(), client->din());
            } else if (arrayF != _arrays.end()) {
                auto cast = new Cast(
                    ptr->getType(),
                    llvm::Type::getInt64Ty(ptr->getContext()));
                conns()->connect(client->dout(), cast->din());
                auto trunc = new IntTruncate(cast->dout()->type(),
                                             iface->din()->type());
                conns()->connect(cast->dout(), trunc->din());
                conns()->connect(trunc->dout(), iface->din());
                conns()->connect(iface->dout(), client->din());
            }
        } else if (store != NULL) {
            auto ptr = store->getPointerOperand();
            unsigned varIdx = resolveMember(ptr);

            auto regF = _regs.find(varIdx);
            auto arrayF = _arrays.find(varIdx);
            auto modF = _modules.find(varIdx);
            assert(modF == _modules.end());
            assert((regF == _regs.end()) != (arrayF == _arrays.end())); 

            auto im = _writePorts[varIdx];
            assert(im != NULL);
            auto iface = im->createServer();
 
            if (regF != _regs.end()) {
                auto extr = new Extract(client->reqType(), {0});
                conns()->connect(client->dout(), extr->din());
                conns()->connect(extr->dout(), iface->din());
                conns()->connect(iface->dout(), client->din());
            } else if (arrayF != _arrays.end()) {
                auto mem = _arrays[varIdx];
                auto split = new Split(client->dout()->type());
                conns()->connect(client->dout(), split->din());
                auto cast = new Cast(
                    ptr->getType(),
                    llvm::Type::getInt64Ty(ptr->getContext()));
                conns()->connect(split->dout(1), cast->din());
                auto trunc = new IntTruncate(
                    cast->dout()->type(),
                    llvm::Type::getIntNTy(ptr->getContext(),
                                          idxwidth(mem->depth()))); 
                conns()->connect(cast->dout(), trunc->din());
                auto join = new Join(iface->din()->type());
                conns()->connect(trunc->dout(), join->din(1));
                conns()->connect(split->dout(0), join->din(0));
                conns()->connect(join->dout(), iface->din());
                conns()->connect(iface->dout(), client->din());
            }
        } else {
            throw InvalidArgument("Don't know how to translate instruction: "
                                  + valuestr(ins));
        }
    }
}

void CPPHDLClass::getGEPChainAsVector(llvm::Value* ptr,
                                      vector<llvm::Value*>& chain) const
{
    assert(ptr->getType()->isPointerTy());

    llvm::Instruction* ins = llvm::dyn_cast_or_null<llvm::Instruction>(ptr);
    if (ins == NULL) {
        fprintf(stderr, "%s\n", valuestr(ptr).c_str());
        throw InvalidArgument(
            "Only instructions which get pointers can be used to refer "
            "to member variables");
    }

    switch(ins->getOpcode()) {
    case llvm::Instruction::Load:
        return getGEPChainAsVector(ins->getOperand(0), chain);
    case llvm::Instruction::GetElementPtr: {
        llvm::GetElementPtrInst* gep =
            llvm::dyn_cast_or_null<llvm::GetElementPtrInst>(ptr);
        if (gep == NULL) {
            throw InvalidArgument("Can only deal with loads determined "
                                  "by GEP instructions! Not this: " +
                                  valuestr(ptr));
        }

        llvm::PointerType* ptrType =
            llvm::dyn_cast<llvm::PointerType>(gep->getPointerOperandType());
        if (ptrType->getPointerElementType() != this->_type) {
            getGEPChainAsVector(gep->getPointerOperand(), chain);
        }

        auto numOperands = gep->getNumOperands();
        for (unsigned i=1; i<numOperands; i++) {
            chain.push_back(gep->getOperand(i));
        }
        return;
    }
    case llvm::Instruction::BitCast:
        // chain.push_back(ins);
        return getGEPChainAsVector(ins->getOperand(0), chain);
    case llvm::Instruction::PHI: {
        llvm::PHINode* phi = llvm::dyn_cast_or_null<llvm::PHINode>(ptr);
        assert(phi != NULL);
        assert(phi->getNumIncomingValues() > 0);
        getGEPChainAsVector(phi->getIncomingValue(0), chain);
        for (unsigned i=1; i<phi->getNumIncomingValues(); i++) {
            vector<llvm::Value*> otherChain;
            getGEPChainAsVector(phi->getIncomingValue(i), otherChain);
            assert(otherChain.size() >= 2);
            assert(otherChain[0] == chain[0]);
            assert(otherChain[1] == chain[1]);
        }
        return;
    }
    default:
        throw InvalidArgument(
            "I dunno how to resolve this value: " + valuestr(ptr));
    }
}

unsigned CPPHDLClass::resolveMember(llvm::Value* ptr) const {
    std::vector<llvm::Value*> chain;
    getGEPChainAsVector(ptr, chain);
    if (chain.size() < 2) {
        throw InvalidArgument(
            "This GEP instruction seems to be attempting to get a "
            "reference to the module. I don't know what a pointer to "
            "silicon looks like but I suspect it's glittery...");
    }

    llvm::ConstantInt* ci;
    
    ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(chain[0]);
    if (ci == NULL || ci->getLimitedValue() != 0) {
        throw InvalidArgument(
            "First in GEP chain must be 0. Higher than that implies "
            "that an array of modules is being indexed, which isn't "
            "supported.");
    }

    ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(chain[1]);
    if (ci == NULL) {
        throw InvalidArgument(
            "First in GEP chain must be a constant integer to index "
            "into the members in the module.");
    } else if (ci->getLimitedValue() > this->_type->getStructNumElements()) {
        throw InvalidArgument(
            "This GEP appears to index past the end of this module. "
            "That's not gonna work for us...");
    } else {
        return ci->getLimitedValue();
    }
}

void CPPHDLClass::adoptSWVersion(std::string name, llvm::Function* func) {
    _swVersion[name] = cloneFunc(func, func->getName().str() + "_sw");
}

void CPPHDLClass::adoptTest(llvm::Function* func) {
    _tests.insert(cloneFunc(func, func->getName()));
}

} // namespace cpphdl
} // namespace llpm
