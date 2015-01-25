#include "design.hpp"

#include <backends/verilog/synthesize.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <passes/transforms/synthesize_mem.hpp>
#include <passes/transforms/synthesize_forks.hpp>
#include <passes/transforms/pipeline.hpp>
#include <passes/print.hpp>
#include <passes/manager.hpp>
#include <passes/transforms/simplify.hpp>
#include <passes/transforms/refine.hpp>
#include <passes/analysis/checks.hpp>
#include <libraries/core/tags.hpp>

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


void Design::buildOpts() {
    _optDesc.add_options()
        ("wedge", value<WedgeEnum>()->default_value(WedgeEnum::Verilator)
                                    ->required(),
            "Selects which wedge to use (e.g. verilator, axi)")
    ;
    _workingDir.addOpts(_optDesc);
}

void Design::notify(variables_map& vm) {
    _workingDir.notify(vm);

    VerilogSynthesizer* vs = new VerilogSynthesizer(*this);
    backend(vs);

    switch (vm["wedge"].as<WedgeEnum>()) {
    case WedgeEnum::Verilator:
        wedge(new VerilatorWedge(vs));
        break;
    case WedgeEnum::AXI:
        wedge(new AXIWedge(*this));
        break;
    }

    elaborations()->append<SynthesizeMemoryPass>();
    elaborations()->append<SynthesizeTagsPass>();
    elaborations()->append<RefinePass>();

    optimizations()->append<SimplifyPass>();
    optimizations()->append<CanonicalizeInputs>();
    // optimizations()->append<SimplifyWaits>();
    optimizations()->append<SimplifyPass>();
    optimizations()->append<FormControlRegionPass>();
    optimizations()->append<SimplifyPass>();
    optimizations()->append<SimplifyPass>();
    optimizations()->append<PipelineDependentsPass>();
    optimizations()->append<GVPrinterPass>();
    optimizations()->append<PipelineCyclesPass>();
    optimizations()->append<LatchUntiedOutputs>();
    optimizations()->append<SynthesizeForksPass>();
    optimizations()->append<CheckConnectionsPass>();
    optimizations()->append<CheckOutputsPass>();
    optimizations()->append<CheckCyclesPass>();
    optimizations()->append<TextPrinterPass>();
    optimizations()->append<StatsPrinterPass>();
}

} // namespace llpm
