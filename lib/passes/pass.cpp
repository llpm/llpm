#include "pass.hpp"

#include <llpm/module.hpp>
#include <llpm/design.hpp>

#include <vector>

using namespace std;

namespace llpm {

bool ModulePass::run() {
    bool ret = false;
    auto mods = _design.modules();
    for (Module* m: mods) {
        if (run(m))
            ret = true;
        m->validityCheck();
    }
    return ret;
}

bool ModulePass::run(Module* mod) {
    uint64_t ctr = mod->changeCounter();
    runInternal(mod);
    vector<Module*> submodules;
    mod->submodules(submodules);
    for (auto sm: submodules) {
        this->run(sm);
    }
    finalize();
    return mod->changeCounter() > ctr;
}

};
