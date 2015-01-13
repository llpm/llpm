#include "manager.hpp"

#include <llpm/module.hpp>
#include <util/misc.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <boost/format.hpp>

using namespace std;

namespace llpm {

void PassManager::debug(Pass* p, Module* mod) {
    unsigned ctr = _debugCounter[mod];
    string fn = str(boost::format("%1%_%2%%3$03u.gv")
                        % mod->name()
                        % _name
                        % ctr);

    auto f =_design.workingDir()->create(fn);
    _design.gv()->writeModule(
        f,
        mod,
        false,
        "After pass " + cpp_demangle(typeid(*p).name()));
    _debugCounter[mod]++;
    f->close();
}

bool PassManager::run(bool debug) {
    bool ret = false;
    for (Pass* p: _passes) {
        printf("== Pass: %s\n", cpp_demangle(typeid(*p).name()).c_str());
        if(p->run()) {
            ret = true;
            if (debug)
                for (Module* mod: _design.modules())
                    this->debug(p, mod);
        }
    }
    return ret;
}

bool PassManager::run(Module* mod, bool debug) {
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

        if (ret && debug)
            this->debug(p, mod);
    }
    mod->validityCheck();
    return ret;
}

};
