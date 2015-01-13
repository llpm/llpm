#include "types.hpp"

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/std_library.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>
#include <llpm/module.hpp>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>

namespace llpm {

llvm::Type* StructTwiddler::GetOutput(llvm::Type* input,
                                      const std::vector<unsigned>& mapping)
{
    std::vector<llvm::Type*> outVec;
    for(unsigned i: mapping) {
        if (i >= input->getStructNumElements())
            throw InvalidArgument("Index in mapping to StructTwiddler exceeds size of input struct");
        outVec.push_back(input->getStructElementType(i));
    }
    return llvm::StructType::get(input->getContext(), outVec);
}

StructTwiddler::StructTwiddler(llvm::Type* input,
                               const std::vector<unsigned>& mapping) :
    Function(input, GetOutput(input, mapping)),
    _mapping(mapping) 
{ }

bool StructTwiddler::refine(ConnectionDB& conns) const
{
    if (_mapping.size() > 0) {
        Split* split = new Split(this->din()->type());
        Join*  join  = new Join(this->dout()->type());

        unsigned ji = 0;
        for (unsigned si: _mapping) {
            conns.connect(split->dout(si), join->din(ji));
            ji += 1;
        }

        conns.remap(this->din(), {split->din()});
        conns.remap(this->dout(), join->dout());
    } else {
        auto empty = new Constant(dout()->type());
        auto wait = new Wait(empty->dout()->type());
        conns.connect(empty->dout(), wait->din());
        conns.remap(dout(), wait->dout());
        conns.remap(din(), wait->newControl(din()->type()));
    }

    return true;
}


llvm::Type* ReplaceElement::GetInput(llvm::Type* input, signed idx) {
    if (idx != -1) {
        return llvm::StructType::get(input, nthType(input, idx), NULL);
    } else {
        unsigned vecLen = numContainedTypes(input);
        llvm::Type* containedType = NULL;
        for (unsigned i=0; i < vecLen; i++) {
            if (i == 0)
                containedType = nthType(input, 0);
            else
                if (containedType != nthType(input, i))
                    throw new InvalidArgument(
                        "Can only dynamically replace elements in completely "
                        "homogeneous types");
        }
        return llvm::StructType::get(
            input, containedType,
            llvm::Type::getIntNTy(containedType->getContext(),
                                  idxwidth(vecLen)),
            NULL); 
    }
}

// Statically selected index
ReplaceElement::ReplaceElement(llvm::Type* ty, signed idx):
    Function(GetInput(ty, idx), ty),
    _idx(idx)
{ }

// Dynamically selected index
ReplaceElement::ReplaceElement(llvm::Type* ty):
    Function(GetInput(ty, -1), ty),
    _idx(-1)
{ }

bool ReplaceElement::refine(ConnectionDB& conns) const {
    auto argSplit = new Split(din()->type());
    auto arr = argSplit->dout(0);
    auto repl = argSplit->dout(1);
    auto arrSplit = new Split(arr->type());
    conns.connect(arr, arrSplit->din());
    auto arrJoin = new Join(arr->type());

    if (_idx == -1) {
        // Dynamic selection.
        auto idx = argSplit->dout(2);
        for (unsigned i=0; i<arrSplit->dout_size(); i++) {
            auto selThis = new IntCompare(idx->type(), idx->type(),
                                          IntCompare::EQ, false);
            auto stJoin = new Join({idx->type(), idx->type()});
            conns.connect(stJoin->dout(), selThis->din());
            conns.connect(idx, stJoin->din(0));
            auto iConst = new Constant(
                llvm::ConstantInt::get(idx->type(), i));
            conns.connect(iConst->dout(), stJoin->din(1));

            auto mux = new Multiplexer(2, arrSplit->dout(i)->type());
            conns.connect(mux->dout(), arrJoin->din(i));
            auto mJoin = new Join(mux->din()->type());
            conns.connect(mJoin->dout(), mux->din());
            conns.connect(selThis->dout(), mJoin->din(0));
            conns.connect(arrSplit->dout(i), mJoin->din(1));
            conns.connect(repl, mJoin->din(2));
        }
    } else {
        // Static selection
        assert(_idx >= 0);
        for (unsigned i=0; i<arrSplit->dout_size(); i++) {
            if (i == (unsigned)_idx) {
                conns.connect(repl, arrJoin->din(i));
            } else {
                conns.connect(arrSplit->dout(i), arrJoin->din(i));
            }
        }
    }
    conns.remap(din(), argSplit->din());
    conns.remap(dout(), arrJoin->dout());
    return true;
}

} // namespace llpm
