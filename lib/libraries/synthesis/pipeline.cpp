#include "pipeline.hpp"

#include <llpm/module.hpp>
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>

using namespace std;

namespace llpm {

PipelineStageController::PipelineStageController(Design& d) :
    Block(),
    _vin(this, llvm::Type::getVoidTy(d.context())),
    _vout(this, llvm::Type::getVoidTy(d.context())),
    _ce(this, llvm::Type::getVoidTy(d.context()))
{ }

void PipelineStageController::connectVin(ConnectionDB* conns, OutputPort* op) {
    if (op->type()->isVoidTy()) {
        conns->connect(op, vin());
        return;
    }
    auto vconst = Constant::getVoid(module()->design());
    auto wait = new Wait(vconst->dout()->type());
    conns->connect(vconst->dout(), wait->din());
    conns->connect(wait->dout(), vin());
    wait->newControl(conns, op);
}

void PipelineRegister::controller(ConnectionDB* conns,
                                  PipelineStageController* controller)
{
    _enable = new InputPort(this,
                            llvm::Type::getVoidTy(_din.type()->getContext()));
    conns->connect(controller->ce(), _enable);
}

} // namespace llpm
