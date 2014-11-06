#include "pass.hpp"

namespace llpm {

void ModulePass::run() {
    auto mods = _design.modules();
    for (Module* m: mods) {
        run(m);
    }
}

};
