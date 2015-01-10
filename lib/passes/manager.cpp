#include "manager.hpp"

#include <llpm/module.hpp>
#include <util/misc.hpp>

namespace llpm {

bool PassManager::run() {
    bool ret = false;
    for (Pass* p: _passes) {
        printf("== Pass: %s\n", cpp_demangle(typeid(*p).name()).c_str());
        if(p->run())
            ret = true;
    }
    return ret;
}

bool PassManager::run(Module* mod) {
    bool ret = false;
    for (Pass* p: _passes) {
        printf("== Pass: %s\n", cpp_demangle(typeid(*p).name()).c_str());
        ModulePass* mp = dynamic_cast<ModulePass*>(p);
        if (mp)
            if (mp->run(mod))
                ret = true;

        LambdaModulePass historypass(_design,
            [p](Module* m) {
                std::set<Block*> blocks;
                m->conns()->findAllBlocks(blocks);
                for (Block* b: blocks) {
                    if (b->history().src() == BlockHistory::Unset) {
                        b->history().setUnknown();
                        b->history().meta(
                            cpp_demangle(typeid(*p).name()));
                    }
                }
            });
        historypass.run(mod);
    }
    mod->validityCheck();
    return ret;
}

};
