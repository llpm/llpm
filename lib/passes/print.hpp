#ifndef __LLPM_PASSES_PRINT_HPP__
#define __LLPM_PASSES_PRINT_HPP__

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

} // namespace llpm

#endif // __LLPM_PASSES_PRINT_HPP__

