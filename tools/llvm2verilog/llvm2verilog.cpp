#include <cstdio>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <llpm/std_library.hpp>
#include <frontends/llvm/translate.hpp>
#include <refinery/refinery.hpp>

using namespace llpm;

int main(int argc, const char** argv) {
    try {
        Design d;
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <llvm_bitcode> [<function_name>*]\n", argv[0]);
            return 1;
        }

        LLVMTranslator trans(d);
        trans.readBitcode(argv[1]);

        vector<Module*> modules;
        for (int i=2; i<argc; i++) {
            modules.push_back(trans.translate(argv[i]));
        }

        BaseLibraryStopCondition<> sc;
        StdLibStops(sc);

        for(auto&& m: modules) {
            // Refine each module until it cannot be further refined
            unsigned passes = m->internalRefine(&sc);
            printf("%u refinement passes on '%s'\n", passes, m->name().c_str());
            if (m->refined(&sc))
                printf("Finished refinement\n");
            else
                printf("Error: could not finish refining\n");
        }
    } catch (Exception& e) {
        fprintf(stderr, "Caught exception!\n\t%s\n", e.msg.c_str());
        return 1;
    }

    return 0;
}
