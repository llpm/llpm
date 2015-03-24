#include "design.hpp"

#include <passes/transforms/synthesize_mem.hpp>
#include <passes/transforms/synthesize_forks.hpp>
#include <passes/transforms/pipeline.hpp>
#include <passes/print.hpp>
#include <passes/manager.hpp>
#include <passes/transforms/simplify.hpp>
#include <passes/transforms/refine.hpp>
#include <passes/analysis/checks.hpp>
#include <libraries/core/tags.hpp>

#include <backends/verilog/synthesize.hpp>
#include <backends/ipxact/ipxact.hpp>
#include <backends/graphviz/graphviz.hpp>

#include <wedges/verilator/verilator.hpp>
#include <wedges/axi/axi_wedge.hpp>

#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>

using namespace boost::program_options;
using namespace std;

namespace llpm {

enum class WedgeEnum {
    Verilator,
    AXI
};
char const* WedgeEnumStrings [] = {
    "verilator",
    "axi"
};
ENUM_SER(WedgeEnum, WedgeEnumStrings);

enum class WrapperEnum {
    None,
    AXI
};
char const* WrapperEnumStrings [] = {
    "none",
    "axi"
};
ENUM_SER(WrapperEnum, WrapperEnumStrings);

enum class BackendEnum {
    Verilog,
    IPXACT
};
char const* BackendEnumStrings [] = {
    "verilog",
    "ipxact"
};
ENUM_SER(BackendEnum, BackendEnumStrings);

void Design::buildOpts() {
    _optDesc.add_options()
        ("wedge", value<WedgeEnum>()->default_value(WedgeEnum::Verilator)
                                    ->required(),
            "Selects which wedge to use (e.g. verilator, axi)")
        ("wrapper", value<WrapperEnum>()->default_value(WrapperEnum::None)
                                    ->required(),
            "Selects a wrapper (e.g. none, axi)")
        ("backend", value<BackendEnum>()->default_value(BackendEnum::Verilog)
                                    ->required(),
            "Selects which backend to use (e.g. verilog, ipxact)")
        ("clk", value<float>()->default_value(-1.0)
                              ->required(),
            "Pipeline the design targeting this clock frequency, in MHz")
        ("control_regions", value<bool>()->default_value(true)
                                         ->required(),
            "Controls whether or not control regions are built")
    ;
    _workingDir.addOpts(_optDesc);
}

void Design::notify(variables_map& vm) {
    _workingDir.notify(vm);

    switch (vm["backend"].as<BackendEnum>()) {
    case BackendEnum::Verilog:
        backend(new VerilogSynthesizer(*this));
        break;
    case BackendEnum::IPXACT:
        backend(new IPXactBackend(*this));
        break;
    }

    bool axiWedge = false;
    switch (vm["wedge"].as<WedgeEnum>()) {
    case WedgeEnum::Verilator:
        wedge(new VerilatorWedge(*this));
        break;
    case WedgeEnum::AXI: {
        auto aw = new AXIWedge(*this);
        wedge(aw);
        wrapper(aw);
        axiWedge = true;
        break;
    }
    }

    switch (vm["wrapper"].as<WrapperEnum>()) {
    case WrapperEnum::None:
        break;
    case WrapperEnum::AXI:
        if (!axiWedge)
            wrapper(new AXIWedge(*this));
        break;
    }

    float clkFreq = vm["clk"].as<float>() * 1e6;

    elaborations()->append<SynthesizeMemoryPass>();
    elaborations()->append<SynthesizeTagsPass>();
    elaborations()->append<RefinePass>();

    // optimizations()->append<SimplifyPass>();
    // optimizations()->append<CanonicalizeInputs>();
    // optimizations()->append<SimplifyWaits>();
    // optimizations()->append<SimplifyPass>();

    if (vm["control_regions"].as<bool>()) {
        optimizations()->append<SimplifyPass>();
        optimizations()->append<FormControlRegionPass>();
    }
    optimizations()->append<SimplifyPass>();
    optimizations()->append<PipelineDependentsPass>();
    // optimizations()->append<GVPrinterPass>();

    optimizations()->append<LatchUntiedOutputs>(clkFreq > 0.0);
    optimizations()->append<SynthesizeForksPass>(clkFreq > 0.0);

    if (clkFreq > 0.0) {
        Time period = Time::s(1.0 / clkFreq);
        optimizations()->append<PipelineFrequencyPass>(period);
    }
    optimizations()->append<PipelineCyclesPass>();

    optimizations()->append<CheckConnectionsPass>();
    optimizations()->append<CheckOutputsPass>();
    optimizations()->append<CheckCyclesPass>();
    optimizations()->append<TextPrinterPass>();
    optimizations()->append<StatsPrinterPass>();
}

} // namespace llpm
