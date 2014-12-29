#include "manager.hpp"

#include <llpm/module.hpp>

namespace llpm {

bool PassManager::run() {
    bool ret = false;
    for (Pass* p: _passes) {
        if(p->run())
            ret = true;
    }
    return ret;
}

bool PassManager::run(Module* mod) {
    bool ret = false;
    for (Pass* p: _passes) {
        ModulePass* mp = dynamic_cast<ModulePass*>(p);
        if (mp)
            if (mp->run(mod))
                ret = true;
    }
    mod->validityCheck();
    return ret;
}

};
