#include "verilator.hpp"

#include <boost/format.hpp>

namespace llpm {

static const char* globalOpts =
    "--cc --compiler clang";

void VerilatorWedge::writeModule(FileSet& fileset, Module* mod) {
    // Write verilog module
    FileSet::File* vModFile = fileset.create(mod->name() + ".v");
    _verilog->writeModule(vModFile, mod);

    // Run Verilator, creating several files
    std::string tmpdir = fileset.tmpdir();
    std::string command = str(
        boost::format("%1%/verilator/bin/verilator_bin %2% --Mdir %3% %4%")
            % Directories::executablePath()
            % globalOpts
            % tmpdir
            % vModFile->name()
            );
    printf("Running: %s\n", command.c_str());
    int rc = system(command.c_str());
    if (rc == -1)
        throw SysError("running verilator");

    vModFile->erase();
}

};

