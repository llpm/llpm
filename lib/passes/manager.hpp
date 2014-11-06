#ifndef __LLPM_PASSES_MANAGER_HPP__
#define __LLPM_PASSES_MANAGER_HPP__

#include <passes/pass.hpp>

#include <deque>

namespace llpm {

class PassManager {
protected:
    Design& _design;
    std::deque<Pass*> _passes;

public:
    PassManager(Design& design) :
        _design(design)
    { }

    void append(Pass* p) {
        _passes.push_back(p);
    }

    void prepend(Pass* p) {
        _passes.push_front(p);
    }

    void run();
    void run(Module* mod);
};

};

#endif // __LLPM_PASSES_MANAGER_HPP__
