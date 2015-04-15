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
        Design d;
        LLVMTranslator trans(d);

        string inputFN, modName;

        po::options_description desc("CPPHDL Options");
        desc.add_options()
            ("help", "Show this output")
            ("input,i", po::value<string>(&inputFN)->required(),
                  "Filename of input bitcode")
            ("module,m", po::value<string>(&modName)->required(),
                  "Name of top module")
        ;
        po::positional_options_description pd;
        pd.add("input", 1)
          .add("module", 2);

        desc.add(d.optDesc());

        for (int i=1; i<argc; i++) {
            if (strcmp(argv[i], "--help") == 0) {
                cout << desc << "\n";
                return 1;
            }
        }

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                      .options(desc).positional(pd).run(), vm);
        po::notify(vm);
        d.notify(vm);

        trans.readBitcode(inputFN);
        trans.prepare(modName);
        trans.translate();
        auto m = trans.get(modName);
        d.addModule(m);

        return d.go();
    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }
    return 0;
}
