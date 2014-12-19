#include "objects.hpp"

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <frontends/cpphdl/library.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

using namespace std;

namespace llpm {
namespace cpphdl {

CPPHDLClass::CPPHDLClass(Design& design,
                         std::string name,
                         llvm::StructType* ty) :
    ContainerModule(design, name),
    _type(ty) {
    buildVariables();
}

void CPPHDLClass::addMember(std::string name, LLVMFunction* func) {
    addServerInterface(func->call()->din(), func->call()->dout(), name);
    connectMem(func);
    _methods[name] = func;
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
        _writePorts[i] = new InterfaceMultiplexer(array->write());
        _readPorts[i] = new InterfaceMultiplexer(array->read());
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

            if (regF != _regs.end()) {
                auto im = _readPorts[varIdx];
                assert(im != NULL);
                auto iface = im->createServer();
                auto w = new Wait(iface->reqType());
                assert(iface->req() == iface->din());
                auto c = Constant::getVoid(design());
                conns()->connect(c->dout(), w->din());
                conns()->connect(w->dout(), iface->din());
                conns()->connect(client->dout(),
                                 w->newControl(client->dout()->type()));
                conns()->connect(iface->dout(), client->din());
            } else if (arrayF != _arrays.end()) {
                assert(false && "Arrays unsupported right now");
            }
        } else if (store != NULL) {
            auto ptr = store->getPointerOperand();
            unsigned varIdx = resolveMember(ptr);

            auto regF = _regs.find(varIdx);
            auto arrayF = _arrays.find(varIdx);
            auto modF = _modules.find(varIdx);
            assert(modF == _modules.end());
            assert((regF == _regs.end()) != (arrayF == _arrays.end())); 

            if (regF != _regs.end()) {
                auto im = _writePorts[varIdx];
                assert(im != NULL);
                auto iface = im->createServer();
                auto extr = new Extract(client->reqType(), {0});
                conns()->connect(client->dout(), extr->din());
                conns()->connect(extr->dout(), iface->din());
                conns()->connect(iface->dout(), client->din());
            } else if (arrayF != _arrays.end()) {
                assert(false && "Arrays unsupported right now");
            }
        } else {
            throw InvalidArgument("Don't know how to translate instruction: "
                                  + valuestr(ins));
        }
    }
}

unsigned CPPHDLClass::resolveMember(llvm::Value* ptr) const {
    assert(ptr->getType()->isPointerTy());
    llvm::GetElementPtrInst* gep =
        llvm::dyn_cast_or_null<llvm::GetElementPtrInst>(ptr);
    if (gep == NULL)
        throw InvalidArgument("Can only deal with loads determined "
                              "by GEP instructions!");
    llvm::PointerType* ptrType =
        llvm::dyn_cast<llvm::PointerType>(gep->getPointerOperandType());
    if (ptrType->getPointerElementType() == this->_type) {
        // I can deal with this since we are indexing off of my base type
        assert(gep->getNumOperands() >= 3);
        llvm::Value* operand = gep->getOperand(2); 
        llvm::ConstantInt* ci =
            llvm::dyn_cast_or_null<llvm::ConstantInt>(operand);
        if (ci == NULL)
            throw InvalidArgument(
                "Cannot dynamically index into module variables!");
        uint64_t idx = ci->getLimitedValue();
        if (idx >= _variables.size()) {
            throw InvalidArgument(
                "Pointer is out of module bounds!");
        }
        return (unsigned)idx;
    } else {
        return resolveMember(gep->getPointerOperand());
    }
}

} // namespace cpphdl
} // namespace llpm
