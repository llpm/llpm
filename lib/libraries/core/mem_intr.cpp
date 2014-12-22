#include "mem_intr.hpp"

#include <util/misc.hpp>

using namespace std;

namespace llpm {

Memory::Memory(llvm::Type* datatype, llvm::Type* idxType) :
    _type(datatype),
    _write(this, GetWriteReq(datatype, idxType),
           llvm::Type::getVoidTy(datatype->getContext()), true, "write"),
    _read(this, GetReadReq(datatype, idxType), datatype, true, "read")
{
    _writeDeps = {_write.din()};
    _readDeps = {_read.din(), _write.din()};
}

llvm::Type* Memory::GetWriteReq(llvm::Type* dt, llvm::Type* idx) {
    if (idx->isVoidTy())
        return dt;
    else
        return llvm::StructType::get(
            dt->getContext(),
            {dt, idx});
}

llvm::Type* Memory::GetReadReq(llvm::Type* dt, llvm::Type* idx) {
    return idx;
}

Register::Register(llvm::Type* ty) :
    Memory(ty, llvm::Type::getVoidTy(ty->getContext()))
{ }

FiniteArray::FiniteArray(llvm::Type* ty, unsigned depth) :
    Memory(ty, llvm::Type::getIntNTy(ty->getContext(), clog2(depth)))
{ }

} // namespace llpm
