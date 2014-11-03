#include "verilator.hpp"

#include <dirent.h>
#include <sys/types.h>

#include <llvm/IR/DerivedTypes.h>

#include <boost/format.hpp>
#include <boost/range/irange.hpp>

#include <util/llvm_type.hpp>

namespace llpm {

static const char* verilatorGlobalOpts =
    "--cc --compiler clang --stats -O3 --trace";

static const char* verilatedCppOpts = 
    "-DVL_PRINTF=printf -DVM_TRACE=1 -DVM_COVERAGE=0 -fbracket-depth=4096";

static const std::vector<std::string> externalFiles = {
    // Verilator crap
    "/verilator/share/verilator/include/verilated.h",
    "/verilator/share/verilator/include/verilatedos.h",
    "/verilator/share/verilator/include/verilated_config.h",
    "/verilator/share/verilator/include/verilated_vcd_c.h",
    "/verilator/share/verilator/include/verilated_imp.h",
    "/verilator/share/verilator/include/verilated_heavy.h",
    "/verilator/share/verilator/include/verilated_syms.h",
    "/verilator/share/verilator/include/verilated.cpp",
    "/verilator/share/verilator/include/verilated_vcd_c.cpp",

    // Verilog libraries
    "/support/backends/verilog/select.v",
};


// Helper function to get directory entries
static int getdir (std::string dir, std::vector<std::string> &files)
{
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        files.push_back(string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}

static void run(std::string command) {
    printf("Running: %s\n", command.c_str());
    int rc = system(command.c_str());
    if (rc == -1)
        throw SysError("running external command");
#if 0
    if (rc != 0)
        throw ExternalError(str(boost::format("Got code %1% while running command: %2%")
                                % rc
                                % command));
#endif
}

void VerilatorWedge::writeModule(FileSet& fileset, Module* mod) {
    // Write verilog module
    FileSet::File* vModFile = fileset.create(mod->name() + ".v");
    _verilog->writeModule(vModFile, mod);

    std::list<FileSet::File*> cppFiles;
    std::list<FileSet::File*> verilatedHppFiles;
    std::list<FileSet::File*> vFiles;
    vFiles.push_back(vModFile);

    // Copy in the necessary globals
    for (auto f: externalFiles) {
        auto ext = f.substr(f.find_last_of("."));
        auto cpy = fileset.copy(Directories::executablePath() + f);
        if (ext == ".cpp")
            cppFiles.push_back(cpy);
        if (ext == ".h")
            verilatedHppFiles.push_back(cpy);
        if (ext == ".v")
            vFiles.push_back(cpy);
    }

    std::string verilogFiles = "";
    for(auto f: vFiles) {
        verilogFiles = verilogFiles + f->name() + " ";
    }

    // Run Verilator, creating several files
    std::string tmpdir = fileset.tmpdir();
    run(str(
        boost::format("%1%/verilator/bin/verilator_bin %2% --Mdir %3% --top-module %4% %5%")
            % Directories::executablePath()
            % verilatorGlobalOpts
            % tmpdir
            % mod->name()
            % verilogFiles
            ));

    // Don't need the verilog output anymore
    vModFile->erase();

    // Copy in some of the verilator outputs
    std::vector<std::string> verilatorOutputs;
    int rc = getdir(tmpdir, verilatorOutputs);
    assert(rc == 0);
    for (auto f: verilatorOutputs) {
        auto ext = f.substr(f.find_last_of("."));
        if (ext == ".cpp") {
            auto cppFile = fileset.copy(tmpdir + "/" + f);
            cppFiles.push_back(cppFile);
        }
        if (ext == ".h") {
            auto verilatedHpp = fileset.copy(tmpdir + "/" + f);
            verilatedHppFiles.push_back(verilatedHpp);
        }
    }

    FileSet::File* hpp = fileset.create(mod->name() + ".hpp");
    writeHeader(hpp, mod);
    FileSet::File* cpp = fileset.create(mod->name() + ".cpp");
    writeImplementation(cpp, mod);
    cppFiles.push_back(cpp);

    // Run clang++ on wedge, compiling to llvm
    std::list<FileSet::File*> objFiles;
    for (auto f: cppFiles) {
        FileSet::File* objFile = f->deriveExt(".bc");
        run(str(
            boost::format("%1%/llvm/bin/clang++ -c -emit-llvm -O0 -o %2% %3% %4%")
                % Directories::executablePath()
                % objFile->name()
                % f->name()
                % verilatedCppOpts
                ));
        objFiles.push_back(objFile);
    }

    FileSet::File* wedgeBC = fileset.create(mod->name() + "_verilator.bc");
    string fileList = "";
    for (auto f: objFiles)
        fileList += f->name() + " ";
    run(str(
        boost::format("%1%/llvm/bin/llvm-link -o %2% %3%")
            % Directories::executablePath()
            % wedgeBC->name()
            % fileList));
    
    // Delete unnecessary files
    fileset.erase(cppFiles);
    fileset.erase(verilatedHppFiles);
    fileset.erase(objFiles);
}

static const std::string HppHeader = R"STRING(
/*****************
 *  This code autogenerated by LLPM.
 *  DO NOT MODIFY MANUALLY!
 *  Manual changes will be overwritten.
 ******/

)STRING";

std::string argName(int i) {
    if (i < 0)
        return "";
    return str(boost::format("arg%1%") % i);
}

std::string typeSigPlain(llvm::Type* type, bool pointerize) {
    if (pointerize) {
        return typeSigPlain(type, false) + "* ";
    }

    switch (type->getTypeID()) {
    case llvm::Type::IntegerTyID:
        if (type->getIntegerBitWidth() <= 8)
            return "uint8_t";
        if (type->getIntegerBitWidth() <= 16)
            return "uint16_t";       
        if (type->getIntegerBitWidth() <= 32)
            return "uint32_t";
        if (type->getIntegerBitWidth() <= 64)
            return "uint64_t";
        throw InvalidArgument("Error: cannot produce C++ header for integer type > 64 bits wide");
    case llvm::Type::HalfTyID:
            return "half";
    case llvm::Type::FloatTyID:
            return "float";
    case llvm::Type::DoubleTyID:
            return "double";
    case llvm::Type::X86_FP80TyID:
            return "_float128";
    case llvm::Type::StructTyID:
        {
            llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
            if (!st->hasName())
                throw InvalidArgument("Can only print struct signature if it has a name!");
            return st->getName();
        }
    default:
        throw InvalidArgument("Error: don't know how to print type: " + typestr(type));
    }

    return typestr(type);
}

std::string typeSig(llvm::Type* type, bool pointerize, bool names) {
    if (type->isStructTy()) {
        llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
        std::string sig = "\n";
        for (auto i: boost::irange<size_t>(0, st->getNumElements())) {
            auto arg = names ? (" " + argName(i)) : "";
            sig = sig + "        " + typeSigPlain(st->getContainedType(i), pointerize) + arg +
                  + (i == (st->getNumElements() - 1) ? "  // " : ", // ") + typestr(st->getContainedType(i)) + "\n";
        }
        return sig;
    }
    return typeSigPlain(type, pointerize) + (names ? (" " + argName(0)) : "");
}

unsigned numArgs(llvm::Type* type) {
    if (type->isStructTy()) {
        llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
        return st->getNumElements();
    }
    return 1;
}

void VerilatorWedge::writeHeader(FileSet::File* f, Module* mod) {
    ostream& os = f->openStream();
    os << HppHeader;
    os << "#ifndef __MOD_" << mod->name() << "_LLPM_WEDGE__\n";
    os << "#define __MOD_" << mod->name() << "_LLPM_WEDGE__\n\n";

    os << "#include <string>\n"
       << "#include <stdint.h>\n\n";

    os << "// Forward declaration for ugly verilator classes\n";
    os << "class V" << mod->name() << ";\n";
    os << "class VerilatedVcdC;\n\n";

    os << "/*\n"
       << " * This class is the wrapper for the " + mod->name() + " module.\n"
       << " */\n";


    os << "class " << mod->name() << " {\n";
    os << "public:\n";

    os << "    " << mod->name() << "();\n"
       << "    ~" << mod->name() << "();\n"
       << "\n";

    for (InputPort* ip: mod->inputs()) {
        os << "    // Input port '" << ip->name() << "'\n";
        auto sig = typeSig(ip->type(), false, false);
        os << boost::format("    bool %1%_nonblock(%2%    );\n")
                    % ip->name()
                    % sig;
        os << boost::format("    void %1%(%2%    );\n")
                    % ip->name()
                    % sig;
        os << "\n";
    }

    for (OutputPort* op: mod->outputs()) {
        os << "    // Output port '" << op->name() << "'\n";
        auto sig = typeSig(op->type(), true, false);
        os << boost::format("    bool %1%_nonblock(%2%    );\n")
                    % op->name()
                    % sig;
        os << boost::format("    void %1%(%2%    );\n")
                    % op->name()
                    % sig;
        os << "\n";
    }


    os << "    // Simulation run control\n"
       << "    void reset();\n"
       << "    void run(unsigned cycles = 1);\n"
       << "    void trace(std::string fn);\n";


    os << "\nprivate:\n";
    os << "    V" << mod->name() << "* simulator;\n";
    os << "    uint64_t cycleCount;\n";
    os << "    VerilatedVcdC* tfp;\n";
    os << "};\n";

    os << "\n"
       << "#endif // __MOD_" << mod->name() << "_LLPM_WEDGE__\n\n";

    f->close();
}

void VerilatorWedge::writeImplementation(FileSet::File* f, Module* mod) {
    ostream& os = f->openStream();
    os << HppHeader;

    os << "#include \"" << mod->name() << ".hpp\"\n"
       << "#include \"V" << mod->name() << ".h\"\n"
       << "#include \"verilated_vcd_c.h\"\n"
       << "\n";

    os << mod->name() << "::" << mod->name() << "() {\n"
       << "    simulator = new V" << mod->name() << "();\n"
       << "    cycleCount = 0;\n"
       << "    tfp = NULL;\n"
       << "};\n";

    os << mod->name() << "::~" << mod->name() << "() {\n"
       << "    if (tfp) {\n"
       << "        tfp->close();\n"
       << "        delete tfp;\n"
       << "    }\n"
       << "    delete simulator;\n"
       << "};\n";

    os << "void " << mod->name() << "::run(unsigned cycles) {"
       << R"STRING(
    for (unsigned i=0; i<cycles; i++) {
        simulator->clk = 1;
        simulator->eval();
        if (this->tfp)
            tfp->dump(this->cycleCount * 2);
        simulator->clk = 0;
        simulator->eval();
        if (this->tfp)
            tfp->dump((this->cycleCount * 2) + 1);
        this->cycleCount += 1;
    }
    if (this->tfp && (this->cycleCount % 10) == 0)
        this->tfp->flush();
}
)STRING";

    os << "void " << mod->name() << "::reset() {"
       << R"STRING(
    simulator->resetn = 0;
    this->run(5); // Five cycle reset -- totally arbitrary

    simulator->resetn = 1;
)STRING";
    for (auto op: mod->outputs()) {
        os << "    simulator->" << op->name() << "_bp = 1;\n";
    }
    os << R"STRING(
    this->run(5); // Five cycles after reset -- totally arbitrary
}
)STRING";

    os << "void " << mod->name() << "::trace(std::string fn) {"
       << R"STRING(
   Verilated::traceEverOn(true);
   this->tfp = new VerilatedVcdC;
   this->simulator->trace(this->tfp, 99);
   this->tfp->open(fn.c_str());
}
)STRING";

    // Code for packing inputs
    for (InputPort* ip: mod->inputs()) {
        os << "// Input port '" << ip->name() << "'\n";
        auto sig = typeSig(ip->type(), false, true);
        os << boost::format("bool %3%::%1%_nonblock(%2%    ) {\n")
                    % ip->name()
                    % sig
                    % mod->name();
        os << boost::format(
R"STRING(
    if ((simulator->%1%_bp & 1) == 1)
        return false;
    simulator->%1%_valid = 1;
)STRING") % ip->name();

        auto type = ip->type();
        auto args = numArgs(type);
        if (args == 1) {
            os << boost::format("    simulator->%1% = arg0;\n")
                    % ip->name();
        } else {
            unsigned numbits = bitwidth(type);
            unsigned numwords = (numbits + 31) / 32;

            // TODO: I don't think this'll work for oddball structs due to
            // alignment issues. Should be replaced with something which relies
            // less on the C++ compiler, I think.
            
            os << "    union {\n"
               << boost::format("        uint32_t arr[%1%];\n") % numwords
               << "        struct {\n";
            for (unsigned arg=0; arg<args; arg++) {
                auto argType = type->getContainedType(arg);
                auto sig = typeSig(argType, false, false);
                os << "            " << sig << " " << argName(arg) << ";\n";
            }
            os << "        };\n"
               << "    } arg;\n";

            for (unsigned arg=0; arg<args; arg++) {
                os << "    arg." << argName(arg) << " = " << argName(arg) << ";\n";
            }

            os << boost::format("    memcpy(simulator->%1%, arg.arr, %2%);\n")
                        % ip->name()
                        % (4 * numwords);
        }

        os << boost::format(
R"STRING(
    this->run(1);
    simulator->%1%_valid = 0;
    return true;
};
)STRING") % ip->name();

        os << boost::format("void %3%::%1%(%2%    ) {\n")
                    % ip->name()
                    % sig
                    % mod->name();
        os << boost::format("    while(!this->%1%_nonblock(") % ip->name();
        for (unsigned arg=0; arg<args; arg++) {
            os << argName(arg);
            if (arg < (args-1))
                os << ", ";
        }
        os << ")) { this->run(1); }\n"
           << "}\n";
    }

    // Code for unpacking outputs
    for (OutputPort* op: mod->outputs()) {
        os << "// Input port '" << op->name() << "'\n";
        auto sig = typeSig(op->type(), true, true);
        os << boost::format("bool %3%::%1%_nonblock(%2%    ) {\n")
                    % op->name()
                    % sig
                    % mod->name();
        os << boost::format(
R"STRING(
    simulator->%1%_bp = 0;
    bool pulsed = false;
    if ((simulator->%1%_valid & 1) == 0) {
        this->run(1);
        pulsed = true;
        simulator->%1%_bp = 1;
        if ((simulator->%1%_valid & 1) == 0) {
            return false;
        }
    }
)STRING") % op->name();

        auto type = op->type();
        auto args = numArgs(type);
        if (args == 1) {
            os << boost::format("    *arg0 = simulator->%1%;\n")
                    % op->name();
        } else {
            unsigned numbits = bitwidth(type);
            unsigned numwords = (numbits + 31) / 32;

            // TODO: I don't think this'll work for oddball structs due to
            // alignment issues. Should be replaced with something which relies
            // less on the C++ compiler, I think.
            
            os << "    union {\n"
               << boost::format("        uint32_t arr[%1%];\n") % numwords
               << "        struct {\n";
            for (unsigned arg=0; arg<args; arg++) {
                auto argType = type->getContainedType(arg);
                auto sig = typeSig(argType, false, false);
                os << "            " << sig << " " << argName(arg) << ";\n";
            }
            os << "        };\n"
               << "    } arg;\n";

            os << boost::format("    memcpy(arg.arr, simulator->%1%, %2%);\n")
                        % op->name()
                        % numwords;

            for (unsigned arg=0; arg<args; arg++) {
                os << "    " << argName(arg) << " = arg." << argName(arg) << ";\n";
            }
        }

        os << boost::format(
R"STRING(
    if (!pulsed)
        this->run(1);
    simulator->%1%_bp = 1;
    return true;
}
)STRING") % op->name();

        os << boost::format("void %3%::%1%(%2%    ) {\n")
                    % op->name()
                    % sig
                    % mod->name();
        os << boost::format("    while(!this->%1%_nonblock(") % op->name();
        for (unsigned arg=0; arg<args; arg++) {
            os << argName(arg);
            if (arg < (args-1))
                os << ", ";
        }
        os << ")) { }\n"
           << "}\n";
    }

    os << "\n";
    f->close();
}

};

