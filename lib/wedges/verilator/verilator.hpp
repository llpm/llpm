#ifndef __LLPM_VERILATOR_VERILATOR_HPP__
#define __LLPM_VERILATOR_VERILATOR_HPP__

#include <util/files.hpp>
#include <backends/verilog/synthesize.hpp>
#include <wedges/wedge.hpp>

#include <string>

namespace llvm {
    class CallInst;
};

namespace llpm {

class VerilatorWedge : public Wedge {
    VerilogSynthesizer* _verilog; 

    void writeHeader(FileSet::File*, Module*);
    void writeImplementation(FileSet::File*, Module*);
    void writeBodies(Module*);
    void addAssertsToTest(Module*, llvm::Function*);
    void addAssertToTest(Module*, std::string ifaceName,
                         llvm::Function*, llvm::CallInst*);

public:
    VerilatorWedge(Design& design) {
        Backend* designBackend = design.backend();
        VerilogSynthesizer* vs = dynamic_cast<VerilogSynthesizer*>(designBackend);
        if (vs == NULL) {
            vs = new VerilogSynthesizer(design);
        }
        _verilog = vs;
    }

    VerilatorWedge(VerilogSynthesizer* vs) :
        _verilog(vs)
    {
        assert(vs != NULL);
    }

    virtual void writeModule(FileSet& fileset, Module* mod);
};

};

#endif // __LLPM_VERILATOR_VERILATOR_HPP__
