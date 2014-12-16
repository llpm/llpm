#include "pass.hpp"

using namespace std;

namespace llpm {

void ModulePass::run() {
    auto mods = _design.modules();
    for (Module* m: mods) {
        run(m);
        m->validityCheck();
    }
}

void ModulePass::run(Module* mod) {
    runInternal(mod);
    vector<Module*> submodules;
    mod->submodules(submodules);
    for (auto sm: submodules) {
        this->run(sm);
    }
}

};
