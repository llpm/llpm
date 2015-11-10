#include "constant.hpp"
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <libraries/core/std_library.hpp>

#include <llvm/IR/Constants.h>

#include <set>

using namespace std;

namespace llpm {

llvm::Constant* EvalConstant(const ConnectionDB* conns,
                             OutputPort* op,
                             set<OutputPort*> seen);

llvm::Constant* EvalConstant(const ConnectionDB* conns,
                             InputPort* ip,
                             set<OutputPort*> seen) {
    if (conns == nullptr)
        return nullptr;
    auto op = conns->findSource(ip);
    if (op == nullptr)
        return nullptr;
    return EvalConstant(conns, op, seen);
}

llvm::Constant* EvalConstant(const ConnectionDB* conns,
                             OutputPort* op,
                             set<OutputPort*> seen) {
    if (seen.count(op) > 0)
        // Don't follow cycles!
        return nullptr;
    if (op == nullptr)
        // Avoid having to do null checks elsewhere
        return nullptr;
    seen.insert(op);

    auto b = op->owner();

    if (b->is<Constant>()) {
        auto c = b->as<Constant>()->value();
        if (c->getType()->isIntegerTy()) {
            auto i = c->getUniqueInteger();
            i = i.sextOrTrunc(64);
            c = llvm::ConstantInt::get(c->getType()->getContext(), i);
        }
        return c;
    }
    if (b->is<Identity>()) {
        return EvalConstant(conns, b->as<Identity>()->din(), seen);
    }
    if (b->is<Join>()) {
        auto j = b->as<Join>();
        std::vector<llvm::Constant*> ins;
        for (unsigned i=0; i<j->din_size(); i++) {
            auto c = EvalConstant(conns, j->din(i), seen);
            if (c == nullptr) {
                c = llvm::Constant::getNullValue(j->din(i)->type());
            }
            ins.push_back(c);
        }
        auto retType = j->dout()->type();
        if (retType->isStructTy()) {
            return llvm::ConstantStruct::getAnon(ins);
        }
        if (retType->isVectorTy()) {
            return llvm::ConstantVector::get(ins);
        }
        return nullptr;
    }

    if (b->is<Function>()) {
        auto f = b->as<Function>();
        auto cin = EvalConstant(conns, f->din(), seen);

        if (b->is<IntAddition>()) {
            llvm::APInt sum(64, 0, true);
            for (unsigned i=0; i<cin->getType()->getNumContainedTypes(); i++) {
                sum += cin->getAggregateElement(i)->getUniqueInteger();
            }
            sum = sum.sextOrTrunc(64);
            return llvm::ConstantInt::get(f->dout()->type()->getContext(), sum);
        }
        if (b->is<IntSubtraction>()) {
            llvm::APInt sum(64, 0, true);
            for (unsigned i=0; i<cin->getType()->getNumContainedTypes(); i++) {
                if (i == 0)
                    sum += cin->getAggregateElement(i)->getUniqueInteger();
                else
                    sum -= cin->getAggregateElement(i)->getUniqueInteger();
            }
            return llvm::ConstantInt::get(f->dout()->type()->getContext(), sum);
        }
    }

    return nullptr;
}

// If the argument port is constant, try to resolve the value to a
// compile-time constant
llvm::Constant* EvalConstant(const ConnectionDB* conns,
                             OutputPort* op) {
    return EvalConstant(conns, op, {});
}

} // namespace llpm
