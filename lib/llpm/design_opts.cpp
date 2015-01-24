#include "design.hpp"

#include <backends/verilog/synthesize.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <wedges/verilator/verilator.hpp>
#include <passes/transforms/synthesize_mem.hpp>
#include <passes/transforms/synthesize_forks.hpp>
#include <passes/transforms/pipeline.hpp>
#include <passes/print.hpp>
#include <passes/manager.hpp>
#include <passes/transforms/simplify.hpp>
#include <passes/transforms/refine.hpp>
#include <passes/analysis/checks.hpp>
#include <libraries/core/tags.hpp>

#include <boost/program_options.hpp>

using namespace boost::program_options;
using namespace std;

namespace llpm {

void Design::buildOpts() {
    _optDesc.add_options()
    ;
    _workingDir.addOpts(_optDesc);
}

void Design::notify(variables_map& vm) {
    _workingDir.notify(vm);

    VerilogSynthesizer* vs = new VerilogSynthesizer(*this);
    backend(vs);
    wedge(new VerilatorWedge(vs));

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
