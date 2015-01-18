#include "library.hpp"

#include <libraries/core/std_library.hpp>
#include <frontends/llvm/instruction.hpp>
#include <util/llvm_type.hpp>

#include <llvm/IR/Constants.h>

using namespace std;

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
        auto extr = new Extract(mi->din()->type(),
                                {ins->getNumOperands()-1});
        conns.remap(mi->din(), extr->din());
        int widthDiff = bitwidth(pt) - bitwidth(extr->dout()->type());
        OutputPort* valOut = extr->dout();
        if (widthDiff > 0) {
            auto extend = new IntExtend(widthDiff, false, extr->dout()->type());
            conns.connect(extr->dout(), extend->din());
            valOut = extend->dout();
        } else if (widthDiff < 0) {
            auto trunc = new IntTruncate(widthDiff * -1, extr->dout()->type());
            conns.connect(extr->dout(), trunc->din());
            valOut = trunc->dout();
        }
        auto cast = new Cast(valOut->type(), mi->dout()->type());
        conns.connect(valOut, cast->din());
        conns.remap(mi->dout(), cast->dout());

        return true;
    }
};

std::vector<std::shared_ptr<Refinery::Refiner>>
    CPPHDLBaseLibrary::BuildCollection() {
    return {
        make_shared<GetElementPtrRefiner>(),
    };
}


} // namespace cpphdl
} // namespace llpm
