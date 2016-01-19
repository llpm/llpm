#pragma once

#include <backends/verilog/synthesize.hpp>
#include <llpm/scheduled_region.hpp>

namespace llpm {

class ScheduledRegionVerilogPrinter {
    VerilogSynthesizer* _parent;
    ScheduledRegion* _sr;
    FileSet::File* _modFile;
    VerilogSynthesizer::Context _ctxt;

public:
    ScheduledRegionVerilogPrinter(
        VerilogSynthesizer* parent,
        FileSet& dir,
        ScheduledRegion* sr);
    ~ScheduledRegionVerilogPrinter();

    void writeRegion(std::set<FileSet::File*>& files);

private:
    void writeHeader();
    void writeFooter();
    void writeIO();
    void writeStartControl();
    void writeConstants();
    void write(const InputPort*);
    void write(const OutputPort*);
    void write(Block*);
    void write(PipelineRegister*);
    void write(PipelineStageController*);
    void writeCycle(unsigned cycleNum);
    void writeOutputs();

    std::set<const Block*> _written;
};

} // namespace llpm
