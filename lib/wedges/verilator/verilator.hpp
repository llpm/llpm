#ifndef __LLPM_VERILATOR_VERILATOR_HPP__
#define __LLPM_VERILATOR_VERILATOR_HPP__

#include <string>

#include <util/files.hpp>
#include <backends/verilog/synthesize.hpp>

namespace llpm {

class VerilatorWedge {
    VerilogSynthesizer* _verilog; 

    void writeHeader(FileSet::File*, Module*);

public:
    VerilatorWedge(Design& design) :
        _verilog(new VerilogSynthesizer(design))
    { }

    VerilatorWedge(VerilogSynthesizer* vs) :
        _verilog(vs)
    {
        assert(vs != NULL);
    }

    void writeModule(FileSet& fileset, Module* mod);
};

};

#endif // __LLPM_VERILATOR_VERILATOR_HPP__
