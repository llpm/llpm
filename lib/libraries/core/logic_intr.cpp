#include "logic_intr.hpp"

#include <libraries/core/comm_intr.hpp>
#include <llpm/module.hpp>
#include <util/llvm_type.hpp>

#include <boost/foreach.hpp>

#include <llvm/IR/Constants.h>

using namespace std;

namespace llpm {
using namespace llvm;

BooleanLogic::BooleanLogic(unsigned,
                           llvm::Type* inputType,
                           llvm::Type* outputType) :
    Function(inputType, outputType)
{
}

std::string Constant::print() const {
    if (this->_value == NULL)
        return "(null)";
    return valuestr(this->_value);
}

std::string Once::print() const {
    if (this->_value == NULL)
        return "(null)";
    return valuestr(this->_value);
}

static llvm::Constant* getLLVMEquivalent(OutputPort* op);
static llvm::Constant* getLLVMEquivalent(InputPort* ip) {
    auto op = ip->owner()->module()->conns()->findSource(ip);
    return getLLVMEquivalent(op);
}

static llvm::Constant* getLLVMEquivalent(OutputPort* op) {
    assert(op != nullptr);
    auto block = op->owner();
    if (block->is<Constant>())
        return block->as<Constant>()->value();

    if (block->is<Join>()) {
        auto join = block->as<Join>();
        vector<llvm::Constant*> vec;
        for (unsigned i=0; i<join->din_size(); i++) {
            auto ip = join->din(i);
            auto eq = getLLVMEquivalent(ip);
            if (eq == nullptr)
                return nullptr;
            vec.push_back(eq);
        }
        if (join->dout()->type()->isStructTy()) {
            return llvm::ConstantStruct::get(
                llvm::dyn_cast<llvm::StructType>(op->type()),
                vec);
        } else if (join->dout()->type()->isVectorTy()) {
            return llvm::ConstantVector::get(vec);
        } else {
            assert(false && "Unknown join type! Fixme or return null.");
        }
    }

    if (block->is<Identity>()) {
        return getLLVMEquivalent(block->as<Identity>()->din());
    }
    if (block->is<Wait>()) {
        return getLLVMEquivalent(block->as<Wait>()->din());
    }

    if (block->is<Extract>()) {
        auto e = block->as<Extract>();
        auto c = getLLVMEquivalent(e->din());
        if (c == nullptr)
            return nullptr;

        for (auto idx: e->path()) {
            c = c->getAggregateElement(idx);
            if (c == nullptr)
                return nullptr;
        }
        return c;
    }

    if (block->is<Split>()) {
        auto s = block->as<Split>();
        auto c = getLLVMEquivalent(s->din());
        if (c == nullptr)
            return nullptr;
        for (unsigned i=0; i<s->dout_size(); i++) {
            if (op == s->dout(i)) {
                return c->getAggregateElement(i);
            }
        }
        return nullptr;
    }

    return nullptr;
}

Constant* Constant::getEquivalent(OutputPort* op) {
    if (op->owner()->is<Constant>())
        return op->owner()->as<Constant>();
    auto eq = getLLVMEquivalent(op);
    if (eq == nullptr)
        return nullptr;
    return new Constant(eq);
}

} // namespace llpm
