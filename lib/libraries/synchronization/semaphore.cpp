#include "semaphore.hpp"

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <llpm/module.hpp>

using namespace std;

namespace llpm {

Semaphore::Semaphore(Design& d) :
    _wait(this,
          llvm::Type::getVoidTy(d.context()),
          llvm::Type::getVoidTy(d.context()),
          true,
          "wait"),
    _signal(this, 
            llvm::Type::getVoidTy(d.context()),
            "signal")
    { }

bool Semaphore::refine(ConnectionDB& conns) const {
    llvm::Type* voidTy = llvm::Type::getVoidTy(
                            conns.module()->design().context());
    Select* proceedSel = new Select(2, voidTy);
    auto once = Once::getVoid(conns.module()->design());
    conns.connect(once->dout(), proceedSel->din(1));
    conns.remap(&_signal, proceedSel->din(0));

    auto wait = new Wait(voidTy);
    wait->newControl(&conns, proceedSel->dout());
    conns.remap(_wait.din(), wait->din());
    conns.remap(_wait.dout(), wait->dout());

    return true;
}

} // namespace llpm
