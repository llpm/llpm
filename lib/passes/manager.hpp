#ifndef __LLPM_PASSES_MANAGER_HPP__
#define __LLPM_PASSES_MANAGER_HPP__

#include <passes/pass.hpp>

#include <deque>
#include <map>

namespace llpm {

class PassManager {
protected:
    Design& _design;
    std::string _name;
    std::deque<Pass*> _passes;
    std::map<Module*, unsigned> _debugCounter;
    void debug(Pass* p, Module* mod);

public:
    PassManager(Design& design, std::string name="passes") :
        _design(design),
        _name(name)
    { }

    void append(Pass* p) {
        _passes.push_back(p);
    }

    template<typename PASS>
    void append() {
        append(new PASS(_design));
    }

    void prepend(Pass* p) {
        _passes.push_front(p);
    }

    bool run(bool debug = false);
    bool run(Module* mod, bool debug = false);
};

};

#endif // __LLPM_PASSES_MANAGER_HPP__
