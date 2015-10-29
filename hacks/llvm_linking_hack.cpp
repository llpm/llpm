#include <llvm/IR/ValueHandle.h>
#include <llvm/IR/InstrTypes.h>
#include <cassert>

namespace llvm {
    // This appears to be around for no reason other than to create
    // a typeinfo for CallbackVH. It doesn't create one in the LLVM
    // library since LLVM is no-rtti. For some reason, however, I need
    // the typeinfo even though I'm not directly extending CallbackVH.
    // Probably an LLVM class I'm using is in a template or something
    // stupid.
    // 
    // Anyway, LLVM is so painfully non-standard that I have to put
    // this here to avoid linking issues.
    void CallbackVH::anchor() { assert(false); }
    // Same for these. I fucking hate LLVM.
    __attribute__((weak)) UnaryInstruction::~UnaryInstruction() { assert(false); }
    __attribute__((weak)) Instruction::~Instruction() { assert(false); }
    __attribute__((weak)) void User::anchor() { assert(false); }
    __attribute__((weak)) Value::~Value() { assert(false); }
}
