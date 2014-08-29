#ifndef __LLPM_VERILOG_SYNTHESIZE_HPP__
#define __LLPM_VERILOG_SYNTHESIZE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>

// fwd def
namespace llvm {
    class Module;
}

namespace llpm {

class VerilogSynthesizer {
    Design& _design;
public:
    VerilogSynthesizer(Design& design);
    virtual ~VerilogSynthesizer() { }

    void writeModule(std::ostream& os, Module* mod);
    void write(std::ostream& os);
};

} // namespace llpm

#endif // __LLPM_VERILOG_SYNTHESIZE_HPP__
