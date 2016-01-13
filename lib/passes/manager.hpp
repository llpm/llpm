#pragma once

#include <passes/pass.hpp>

#include <deque>
#include <map>
#include <memory>

namespace llpm {

class PassManager {
protected:
    Design& _design;
    std::string _name;
    std::deque<std::shared_ptr<Pass>> _passes;
    std::map<Module*, unsigned> _debugCounter;
    void debug(Pass* p, Module* mod);

public:
    PassManager(Design& design, std::string name="passes") :
        _design(design),
        _name(name)
    { }

    void append(std::shared_ptr<Pass> p) {
        _passes.push_back(p);
    }

    template<typename PASS>
    void append() {
        append(std::make_shared<PASS>(_design));
    }

    template<typename PASS, typename... Args>
    void append(Args... args) {
        append(std::make_shared<PASS>(_design, args...));
    }

    void prepend(std::shared_ptr<Pass> p) {
        _passes.push_front(p);
    }

    bool run(bool debug = false);
    bool run(Module* mod, bool debug = false);
};

};

