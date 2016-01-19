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
    void write(const ScheduledRegion::Cycle*, const InputPort*);
    void write(const ScheduledRegion::Cycle*, const OutputPort*);
    void write(const ScheduledRegion::Cycle*, Block*);
    void write(const ScheduledRegion::Cycle*, DummyBlock*);
    void write(const ScheduledRegion::Cycle*, PipelineRegister*);
    void write(const ScheduledRegion::Cycle*, PipelineStageController*);
    void writeCycle(unsigned cycleNum);
    void writeOutputs();

    std::string validSignal(const ScheduledRegion::Cycle*);

    std::set<const Block*> _written;
};

} // namespace llpm
