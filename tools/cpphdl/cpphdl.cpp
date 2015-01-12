#include <cstdio>
#include <iostream>
#include <fstream>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <libraries/core/std_library.hpp>
#include <frontends/cpphdl/translate.hpp>
#include <refinery/refinery.hpp>
#include <backends/verilog/synthesize.hpp>
#include <backends/graphviz/graphviz.hpp>
#include <wedges/verilator/verilator.hpp>
#include <util/llvm_type.hpp>
#include <passes/transforms/synthesize_mem.hpp>
#include <passes/transforms/synthesize_forks.hpp>
#include <passes/transforms/pipeline.hpp>

#include <passes/manager.hpp>
#include <passes/transforms/simplify.hpp>
#include <passes/transforms/refine.hpp>
#include <passes/analysis/checks.hpp>
#include <libraries/core/tags.hpp>

using namespace llpm;
using namespace cpphdl;
using namespace std;

int main(int argc, const char** argv) {
    try {
        Design d;
        VerilogSynthesizer vs(d);
        d.backend(&vs);

        if (argc < 3) {
            fprintf(stderr, "Usage: %s <llvm_bitcode> <output directory> [<class_name>*]\n", argv[0]);
            return 1;
        }

        string dirName = argv[2];

        {
            CPPHDLTranslator trans(d);
            trans.readBitcode(argv[1]);

            set<Module*> modules;
            for (int i=3; i<argc; i++) {
                auto m = trans.translate(argv[i]);
                modules.insert(m);
                d.addModule(m);
            }
        }

        d.elaborations()->append<SynthesizeMemoryPass>();
        d.elaborations()->append<SynthesizeTagsPass>();
        d.elaborations()->append<RefinePass>();

        d.optimizations()->append<SimplifyPass>();
        d.optimizations()->append<FormControlRegionPass>();
        d.optimizations()->append<SimplifyPass>();
        d.optimizations()->append<PipelineDependentsPass>();
        d.optimizations()->append<PipelineCyclesPass>();
        d.optimizations()->append<SynthesizeForksPass>();
        d.optimizations()->append<CheckConnectionsPass>();
        d.optimizations()->append<CheckOutputsPass>();
        d.optimizations()->append<CheckCyclesPass>();

        FileSet fs(true, dirName, true);
        GraphvizOutput gv(d);
        VerilatorWedge vw(&vs);
 
        // printf("Writing graphviz output...\n");
        // for (Module* mod: d.modules()) {
            // gv.writeModule(fs.create(mod->name() + ".gv"), mod);
        // }
 
        printf("Elaborating...\n");
        d.elaborate();
        printf("Optimizing...\n");
        d.optimize();

        for (Module* mod: d.modules()) {
            printf("Interfaces: \n");
            for (Interface* i: mod->interfaces()) {
                printf("  <-> %s  %s -> %s\n",
                       i->name().c_str(),
                       typestr(i->req()->type()).c_str(),
                       typestr(i->resp()->type()).c_str());
            }

            printf("Ports: \n");
            for (InputPort* ip: mod->inputs()) {
                printf("  -> %s : %s\n",
                       ip->name().c_str(), typestr(ip->type()).c_str());
            }
            for (OutputPort* op: mod->outputs()) {
                printf("  <- %s : %s\n",
                       op->name().c_str(), typestr(op->type()).c_str());
            }

            printf("Writing graphviz output...\n");
            gv.writeModule(fs.create(mod->name() + ".gv"), mod, true);
            gv.writeModule(fs.create(mod->name() + "_simple.gv"), mod, false);
            vector<Module*> submodules;
            mod->submodules(submodules);
            for (auto sm: submodules) {
                gv.writeModule(fs.create(sm->name() + ".gv"), sm, true);
            }

            printf("Writing Verilog output...\n");
            vw.writeModule(fs, mod);
        }

    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }

    return 0;
}
