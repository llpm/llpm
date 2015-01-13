#include <cstdio>
#include <iostream>
#include <fstream>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <llpm/control_region.hpp>
#include <libraries/core/std_library.hpp>
#include <frontends/llvm/translate.hpp>
#include <refinery/refinery.hpp>
#include <backends/verilog/synthesize.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <wedges/verilator/verilator.hpp>

#include <passes/manager.hpp>
#include <passes/transforms/synthesize_mem.hpp>
#include <passes/transforms/synthesize_forks.hpp>
#include <passes/transforms/simplify.hpp>
#include <passes/transforms/refine.hpp>
#include <passes/analysis/checks.hpp>
#include <libraries/core/tags.hpp>

using namespace llpm;
using namespace std;

int main(int argc, const char** argv) {
    try {


        if (argc < 3) {
            fprintf(stderr, "Usage: %s <llvm_bitcode> <output directory> [<function_name>*]\n", argv[0]);
            return 1;
        }

        string dirName = argv[2];
        Design d(dirName, true);
        VerilogSynthesizer vs(d);
        d.backend(&vs);

        LLVMTranslator trans(d);
        trans.readBitcode(argv[1]);

        vector<Module*> modules;
        for (int i=3; i<argc; i++) {
            auto m = trans.translate(argv[i]);
            modules.push_back(m);
            d.addModule(m);
        }

        d.elaborations()->append<SynthesizeMemoryPass>();
        d.elaborations()->append<SynthesizeTagsPass>();
        d.elaborations()->append<RefinePass>();

        d.optimizations()->append<SimplifyPass>();
        d.optimizations()->append<FormControlRegionPass>();
        d.optimizations()->append<SynthesizeForksPass>();
        d.optimizations()->append<CheckConnectionsPass>();

        printf("Elaborating...\n");
        d.elaborate();
        printf("Optimizing...\n");
        d.optimize();

        GraphvizOutput gv(d);
        VerilatorWedge vw(&vs);

        FileSet* fs = d.workingDir();

        for (Module* mod: d.modules()) {
            gv.writeModule(fs->create(mod->name() + ".gv"), mod);
            vw.writeModule(*fs, mod);
        }

    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }

    return 0;
}
