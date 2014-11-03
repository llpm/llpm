#include <cstdio>
#include <iostream>
#include <fstream>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <libraries/core/std_library.hpp>
#include <frontends/llvm/translate.hpp>
#include <refinery/refinery.hpp>
#include <backends/verilog/synthesize.hpp>

#include <wedges/verilator/verilator.hpp>

using namespace llpm;

int main(int argc, const char** argv) {
    try {
        Design d;
        if (argc < 3) {
            fprintf(stderr, "Usage: %s <llvm_bitcode> <output directory> [<function_name>*]\n", argv[0]);
            return 1;
        }

        LLVMTranslator trans(d);
        trans.readBitcode(argv[1]);

        string dirName = argv[2];

        vector<Module*> modules;
        for (int i=3; i<argc; i++) {
            modules.push_back(trans.translate(argv[i]));
        }

        BaseLibraryStopCondition sc;
        StdLibStops(sc);

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

        VerilogSynthesizer vs(d);
        VerilatorWedge vw(&vs);

        FileSet fs(true, dirName, true);
        for (Module* mod: d.modules()) {
            vw.writeModule(fs, mod);
        }

    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }

    return 0;
}
