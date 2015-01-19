#include "memory.hpp"

#include <util/misc.hpp>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

using namespace std;

namespace llpm {

RTLReg::RTLReg(llvm::Type* dt) :
    _type(dt),
    _write(this, dt, llvm::Type::getVoidTy(dt->getContext()), true, "write")
{ }

Interface* RTLReg::newRead() {
    auto iface = new Interface(
                    this,
                    llvm::Type::getVoidTy(_type->getContext()),
                    _type,
                    true,
                    str(boost::format("read%1%") % _read.size()));
    _read.emplace_back(iface);
    _readRespDeps[iface->dout()] = {iface->din()};
    return iface;
}

BlockRAM::BlockRAM(llvm::Type* ty, unsigned depth, unsigned numPorts) :
    _type(ty),
    _depth(depth) {

    auto& ctxt = ty->getContext();
    llvm::Type* reqType =
        llvm::StructType::get(
            ctxt, vector<llvm::Type*>(
                { llvm::Type::getInt1Ty(ctxt),
                  ty, llvm::Type::getIntNTy(ctxt, idxwidth(depth)) }));
    llvm::Type* respType = ty;

    for (unsigned i=0; i<numPorts; i++) {
        auto iface = new Interface(this, reqType, respType, true,
                                   str(boost::format("port%1%") % i));
        _ports.emplace_back(iface);
        _deps[iface->dout()] = {iface->din()};
    }
}

} // namespace llpm
