#include "constant.hpp"
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>

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
        return b->as<Constant>()->value();
    }
    if (b->is<Identity>()) {
        return EvalConstant(conns, b->as<Identity>()->din(), seen);
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
