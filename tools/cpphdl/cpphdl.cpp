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

#include <passes/manager.hpp>
#include <passes/transforms/simplify.hpp>

using namespace llpm;
using namespace cpphdl;

int main(int argc, const char** argv) {
    try {
        Design d;
        VerilogSynthesizer vs(d);
        d.backend(&vs);

        if (argc < 3) {
            fprintf(stderr, "Usage: %s <llvm_bitcode> <output directory> [<class_name>*]\n", argv[0]);
            return 1;
        }

        CPPHDLTranslator trans(d);
        trans.readBitcode(argv[1]);

        string dirName = argv[2];

        set<Module*> modules;
        for (int i=3; i<argc; i++) {
            auto m = trans.translate(argv[i]);
            modules.insert(m);
            d.addModule(m);
        }

        BaseLibraryStopCondition sc;
        StdLibStops(sc);

        PassManager pm(d);
        pm.append(new SimplifyPass(d));

        for(auto&& m: modules) {
            m->validityCheck();

            printf("Module inputs %lu, outputs %lu before refinement\n",
                   m->inputs().size(), m->outputs().size());
            // Refine each module until it cannot be further refined
            unsigned passes = m->internalRefine(&sc);
            printf("%u refinement passes on '%s'\n", passes, m->name().c_str());
            printf("Module inputs %lu, outputs %lu after refinement\n",
                   m->inputs().size(), m->outputs().size());

            vector<Block*> blocks;
            m->blocks(blocks);

            if (m->refined(&sc))
                printf("Finished refinement\n");
            else {
                printf("Error: could not finish refining!\n");
                printf("Remaining blocks to be refined: \n");
                sc.unrefined(blocks);
                for(Block* b: blocks) {
                    printf("\t%s\n", typeid(*b).name());
                }
            }

            m->validityCheck();
        }

        printf("Running optimization passes...\n");
        pm.run();
        printf("Done optimizing.\n");

        GraphvizOutput gv(d);
        VerilatorWedge vw(&vs);


        FileSet fs(true, dirName, true);
        for (Module* mod: d.modules()) {
            printf("Writing graphviz output...\n");
            gv.writeModule(fs.create(mod->name() + ".gv"), mod);
            printf("Writing Verilog output...\n");
            vw.writeModule(fs, mod);
        }

    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }

    return 0;
}