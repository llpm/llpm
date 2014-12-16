#include "library.hpp"

#include <frontends/llvm/instruction.hpp>

#include <llvm/IR/Constants.h>

namespace llpm {
namespace cpphdl {

class GetElementPtrRefiner : public BlockRefiner {
    virtual bool handles(Block* b) const {
        auto mi = dynamic_cast<LLVMMiscInstruction*>(b);
        if (mi == NULL)
            return false;
        llvm::Instruction* ins = mi->ins();
        return ins->getOpcode() == llvm::Instruction::GetElementPtr;
    }

    virtual bool refine(
            const Block* block,
            ConnectionDB& conns) const
    {
        auto mi = dynamic_cast<const LLVMMiscInstruction*>(block);
        if (mi == NULL)
            return false;
        llvm::Instruction* ins = mi->ins();
        if (ins->getOpcode() != llvm::Instruction::GetElementPtr)
            return false;

        assert(ins->getType()->isPointerTy());
        auto pt = llvm::dyn_cast<llvm::PointerType>(ins->getType());
        auto c = new Constant(llvm::ConstantPointerNull::get(pt));
        auto w = new Wait(ins->getType());
        conns.connect(c->dout(), w->din());
        conns.remap(mi->dout(), w->dout());

        for (auto ip: mi->inputs()) {
            auto newIP = w->newControl(ip->type());
            conns.remap(ip, newIP);
        }

        return true;
    }
};

std::vector<Refinery::Refiner*> CPPHDLBaseLibrary::BuildCollection() {
    return {
        new GetElementPtrRefiner(),
    };
}


} // namespace cpphdl
} // namespace llpm
