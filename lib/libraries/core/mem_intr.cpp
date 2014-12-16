#include "mem_intr.hpp"

#include <util/misc.hpp>

using namespace std;

namespace llpm {

Register::Register(llvm::Type* ty) :
    _write(this, ty, llvm::Type::getVoidTy(ty->getContext()), "write")
{ }

Register::~Register() {
    for (auto iface: _read) {
        delete iface;
    }
}

Interface* Register::newRead() {
    auto iface =
        new Interface(this,
                      llvm::Type::getVoidTy(_write.cin()->type()->getContext()),
                      _write.cin()->type(),
                      str(boost::format("read%1%") % _read.size()));
    _read.push_back(iface);
    return iface;
}

llvm::Type* Array::GetWriteInputType(llvm::Type* type, unsigned depth) {
    return llvm::StructType::get(
        type->getContext(),
        {type, llvm::Type::getIntNTy(type->getContext(), clog2(depth))});
}

llvm::Type* Array::GetReadInputType(llvm::Type* type, unsigned depth) {
    return llvm::Type::getIntNTy(type->getContext(), clog2(depth));
}

llvm::Type* Array::GetReadOutputType(llvm::Type* type, unsigned depth) {
    return type;
}

Array::Array(llvm::Type* ty, unsigned depth) :
    _write(this, GetWriteInputType(ty, depth),
           llvm::Type::getVoidTy(ty->getContext()), "write"),
    _read(this, GetReadInputType(ty, depth),
          GetReadOutputType(ty, depth), "read")
{ }

} // namespace llpm
