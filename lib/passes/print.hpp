#pragma once

#include <passes/pass.hpp>
#include <map>

namespace llpm {

class GVPrinterPass: public ModulePass {
    std::map<Module*, unsigned> _counter;
    std::string _name;
public:
    GVPrinterPass(Design& design, std::string name=""):
        ModulePass(design),
        _name(name)
    { }

    virtual void runInternal(Module* mod);
};

class TextPrinterPass: public ModulePass {
    std::map<Module*, unsigned> _counter;
    std::string _name;
public:
    TextPrinterPass(Design& design, std::string name=""):
        ModulePass(design),
        _name(name)
    { }

    virtual void runInternal(Module* mod);
};

class StatsPrinterPass: public ModulePass {
    std::map<std::string, uint64_t> _typeCounters;
    std::string _name;
public:
    StatsPrinterPass(Design& design, std::string name=""):
        ModulePass(design),
        _name(name)
    { }

    virtual void runInternal(Module* mod);
    virtual void finalize();
};

} // namespace llpm


