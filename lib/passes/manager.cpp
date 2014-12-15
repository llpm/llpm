#include "manager.hpp"

namespace llpm {

void PassManager::run() {
    for (Pass* p: _passes) {
        p->run();
    }
}

void PassManager::run(Module* mod) {
    for (Pass* p: _passes) {
        ModulePass* mp = dynamic_cast<ModulePass*>(p);
        if (mp)
            mp->run(mod);
    }
    mod->validityCheck();
}

};
