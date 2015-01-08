#include "verilator.hpp"

#include <dirent.h>
#include <sys/types.h>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Linker/Linker.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <boost/format.hpp>
#include <boost/range/irange.hpp>

#include <util/misc.hpp>
#include <util/llvm_type.hpp>

using namespace std;

namespace llpm {

static const char* verilatorGlobalOpts =
    "--cc --stats --compiler clang -O3 --trace --assert --x-assign unique";

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
    "/support/backends/verilog/pipeline.v",
    "/support/backends/verilog/memory.v",
    "/support/backends/verilog/fork.v",
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

    // Load all the output objs and merge them into swModule
    llvm::Module* merged = NULL;
    string errMsg;
    for (auto f: objFiles) {
        llvm::SMDiagnostic Err;
        llvm::Module* m = llvm::ParseIRFile(
            f->name(), Err, mod->design().context());
        if (m == NULL) {
            Err.print("", llvm::errs());
            assert(false);
        }

        if (merged == NULL) {
            merged = m;
        } else {
            llvm::Linker L(merged, false);
            auto rc = L.linkInModule(m, &errMsg);
            if (rc) {
                printf("Error linking: %s\n", errMsg.c_str());
                assert(false);
            }
        }
    }

    // Link in the existing shit
    llvm::Linker L(merged, false);
    if (mod->swModule() != NULL) {
        if (L.linkInModule(mod->swModule(), &errMsg)) {
            printf("Error linking: %s\n", errMsg.c_str());
            assert(false);
        }
    }
    mod->swModule(merged);

    // Some of the functions are implemented directly in IR
    writeBodies(mod);

    // Output the code associated with this module
    llvm::Module* swModule = mod->swModule();
    if (swModule != NULL) {
        auto bcFile = fileset.create(mod->name() + "_sw.bc");
        llvm::raw_os_ostream llvmStream(bcFile->openStream());
        llvm::WriteBitcodeToFile(swModule, llvmStream);
    }

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

std::string argName(llvm::Type* ty, int i) {
    if (ty->isVoidTy())
        return "";
    if (i < 0)
        return "";
    return str(boost::format("arg%1%") % i);
}

bool implicitPointer(llvm::Type* ty) {
    return ty->getTypeID() == llvm::Type::VectorTyID ||
           ty->getTypeID() == llvm::Type::ArrayTyID;
}

std::pair<string, string> typeSigPlain(llvm::Type* type, bool pointerize) {
    if (pointerize && !implicitPointer(type)) {
        auto p = typeSigPlain(type, false);
        return make_pair(p.first + "* ", p.second);
    }

    switch (type->getTypeID()) {
    case llvm::Type::IntegerTyID:
        if (type->getIntegerBitWidth() <= 8)
            return make_pair("uint8_t", "");
        if (type->getIntegerBitWidth() <= 16)
            return make_pair("uint16_t", "");
        if (type->getIntegerBitWidth() <= 32)
            return make_pair("uint32_t", "");
        if (type->getIntegerBitWidth() <= 64)
            return make_pair("uint64_t", "");
        throw InvalidArgument("Error: cannot produce C++ header for integer type > 64 bits wide");
    case llvm::Type::HalfTyID:
            return make_pair("half", "");
    case llvm::Type::FloatTyID:
            return make_pair("float", "");
    case llvm::Type::DoubleTyID:
            return make_pair("double", "");
    case llvm::Type::X86_FP80TyID:
            return make_pair("_float128", "");
    case llvm::Type::PointerTyID:
            return make_pair("uint64_t", "");
    case llvm::Type::VoidTyID:
            return make_pair("void", "");
    case llvm::Type::VectorTyID: {
            auto p = typeSigPlain(nthType(type, 0), false);
            return make_pair(
                p.first,
                p.second + str(boost::format("[%1%]")
                                % numContainedTypes(type)));
    }
    case llvm::Type::StructTyID:
        {
            llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
            if (!st->hasName())
                throw InvalidArgument("Can only print struct signature if it has a name!");
            return make_pair(st->getName(), "");
        }
    default:
        throw InvalidArgument("Error: don't know how to print type: " + typestr(type));
    }

    return make_pair(typestr(type), "");
}

std::string typeSig(llvm::Type* type, bool pointerize, int name) {
    if (type->isStructTy()) {
        llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
        std::string sig = "\n";
        for (auto i: boost::irange<size_t>(0, st->getNumElements())) {
            auto ty = st->getContainedType(i);
            auto arg = name >= 0 ? (" " + argName(ty, i)) : "";
            auto p = typeSigPlain(ty, pointerize);
            sig = sig + "        " + p.first + arg + p.second +
                  + (i == (st->getNumElements() - 1) ?
                        "  // " : ", // ") +
                  typestr(st->getContainedType(i)) + "\n";
        }
        return sig;
    }
    auto p = typeSigPlain(type, pointerize);
    return p.first +
        (name >= 0 ? (" " + argName(type, name)) : "") + p.second;
}

unsigned numArgs(llvm::Type* type) {
    if (type->isStructTy()) {
        llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
        return st->getNumElements();
    }
    return 1;
}

llvm::Type* argType(unsigned i, llvm::Type* type) {
    if (type->isStructTy()) {
        llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(type);
        return st->getStructElementType(i);
    } else {
        if (i > 0)
            throw InvalidArgument(
                "Can only request argType > 0 for struct types");
        return type;
    }
}

void writeStruct(ostream& os, llvm::Type* type, string name) {
    // TODO: I don't think this'll work for oddball structs due to
    // alignment issues. Should be replaced with something which relies
    // less on the C++ compiler, I think.
    auto args = numArgs(type);
    unsigned numbits = bitwidth(type);

    unsigned numwords = (numbits + 31) / 32;
    os << "    union " << name << " {\n"
       << boost::format("        uint32_t arr[%1%];\n") % numwords
       << "        struct {\n";
    if (!type->isVoidTy()) {
        for (unsigned arg=0; arg<args; arg++) {
            auto argType = nthType(type, arg);
            if (bitwidth(argType) > 0) {
                auto sig = typeSig(argType, false, arg);
                os << "            " << sig << ";\n";
            }
        }
    }
    os << "        };\n"
       << "    };\n";
}

std::string structName(Port* p, string prefix="") {
    llvm::Type* ty = p->type();
    if (ty->isSingleValueType() && !implicitPointer(ty)) {
        auto pr = typeSigPlain(ty, false);
        return pr.first + pr.second;
    } else if (ty->isStructTy() &&
               ty->getStructNumElements() == 1 &&
               ty->getStructElementType(0)->isSingleValueType() &&
               !implicitPointer(ty->getStructElementType(0))) {
        auto pr = typeSigPlain(ty->getStructElementType(0), false);
        return pr.first + pr.second;
    } else {
        return prefix + p->name() + "_type";
    }
}

void VerilatorWedge::writeHeader(FileSet::File* f, Module* mod) {
    ostream& os = f->openStream();
    os << HppHeader;
    os << "#ifndef __MOD_" << mod->name() << "_LLPM_WEDGE__\n";
    os << "#define __MOD_" << mod->name() << "_LLPM_WEDGE__\n\n";

    os << "#include <string>\n"
       << "#include <deque>\n"
       << "#include <stdint.h>\n\n";

    os << "// Forward declaration for ugly verilator classes\n";
    os << "class V" << mod->name() << ";\n";
    os << "class VerilatedVcdC;\n\n";

    os << "/*\n"
       << " * This class is the wrapper for the " + mod->name() + " module.\n"
       << " */\n";


    os << "class " << mod->name() << " {\n";
    os << "public:\n";

    for (InputPort* ip: mod->inputs()) {
        string tName = ip->name() + "_type";
        writeStruct(os, ip->type(), tName);
    }

    for (OutputPort* op: mod->outputs()) {
        string tName = op->name() + "_type";
        writeStruct(os, op->type(), tName);
    }

    os << "    " << mod->name() << "();\n"
       << "    ~" << mod->name() << "();\n"
       << "\n";

    for (InputPort* ip: mod->inputs()) {
        os << "    // Input port '" << ip->name() << "'\n";
        string tName = ip->name() + "_type";
        auto sig = typeSig(ip->type(), false, -1);
        os << boost::format("    void %1%_pack(%2%);\n")
                    % ip->name()
                    % tName;
        os << boost::format("    void %1%_nonblock(%2%);\n")
                    % ip->name()
                    % tName;
        os << boost::format("    void %1%(%2%);\n")
                    % ip->name()
                    % tName;
       os << boost::format("    void %1%(%2%    );\n")
                    % ip->name()
                    % sig;
        os << "\n";
    }

    for (OutputPort* op: mod->outputs()) {
        os << "    // Output port '" << op->name() << "'\n";
        auto sig = typeSig(op->type(), true, -1);
        string tName = op->name() + "_type";
        os << boost::format("    void %1%_unpack(%2%*);\n")
                    % op->name()
                    % tName;
        os << boost::format("    bool %1%_nonblock(%2%*);\n")
                    % op->name()
                    % tName;
        os << boost::format("    void %1%(%2%*);\n")
                    % op->name()
                    % tName;
        os << boost::format("    void %1%(%2%    );\n")
                    % op->name()
                    % sig;
        os << "\n";
    }

    os << "    // Interfaces\n";
    for (Interface* iface: mod->interfaces()) {
        // Only server interfaces can be translated to calls
        if (iface->server()) {
            auto argSig = typeSig(iface->req()->type(), false, -1);
            auto retSig = structName(iface->dout());
            os << boost::format("    %3% %1%(%2%    );\n")
                        % iface->name()
                        % argSig
                        % retSig;
        }
    }
    os << "\n";

    os << "    // Tests\n";
    for (llvm::Function* test: mod->tests()) {
        auto p = typeSigPlain(test->getReturnType(), false);
        string rt = p.first + p.second;
        os << "    "
           << "bool "
           << cpp_demangle(test->getName().str().c_str()) << ";\n";
    }
    os << "\n";

    os << "    // Simulation run control\n"
       << "    void reset();\n"
       << "    void run(unsigned cycles = 1);\n"
       << "    void trace(std::string fn);\n"
       << "    uint64_t cycles() const { return cycleCount; }\n";

    os << "\nprivate:\n";
    os << "    void prepareOutputs();\n";
    os << "    void readOutputs();\n";
    os << "    void prepareInputs();\n";
    os << "    void writeInputs();\n";
    os << "    V" << mod->name() << "* simulator;\n";
    os << "    uint64_t cycleCount;\n";
    os << "    \n";


    for (InputPort* ip: mod->inputs()) {
        string tName = ip->name() + "_type";
        os << boost::format("    std::deque<%2%> %1%_outgoing;\n")
                    % ip->name()
                    % tName;
        os << "\n";
    }

    for (OutputPort* op: mod->outputs()) {
        string tName = op->name() + "_type";
        os << boost::format("    std::deque<%2%> %1%_incoming;\n")
                    % op->name()
                    % tName;
        os << "\n";
    }

    os << "    \n";
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
        prepareOutputs();
        prepareInputs();

        simulator->clk = 0;
        simulator->eval();
        if (this->tfp)
            tfp->dump(this->cycleCount * 4);

        readOutputs();
        writeInputs();
        simulator->clk = 1;
        simulator->eval();
        if (this->tfp)
            tfp->dump(this->cycleCount * 4 + 1);

        prepareOutputs();
        prepareInputs();

        simulator->clk = 1;
        simulator->eval();
        if (this->tfp)
            tfp->dump(this->cycleCount * 4 + 2);

        simulator->clk = 0;
        simulator->eval();
        if (this->tfp)
            tfp->dump((this->cycleCount * 4) + 3);
        this->cycleCount += 1;
    }
    if (this->tfp && (this->cycleCount % 10) == 0)
        this->tfp->flush();
}
)STRING";

    os << "void " << mod->name() << "::prepareOutputs() {\n";
    for (OutputPort* op: mod->outputs()) {
        auto sig = typeSig(op->type(), false, -1);
        os << boost::format("    if (%1%_incoming.size() >= 10)\n"
                            "        simulator->%1%_bp = 1;\n"
                            "    else\n"
                            "        simulator->%1%_bp = 0;\n"
                            )
                    % op->name();
        os << "\n";
    }
    os << "};\n";

    os << "void " << mod->name() << "::readOutputs() {\n";
    for (OutputPort* op: mod->outputs()) {
        auto sig = typeSig(op->type(), false, -1);
        os << boost::format("    if (simulator->%1%_valid &&\n"
                            "        !simulator->%1%_bp) {\n"
                            "        %1%_incoming.resize(%1%_incoming.size()+1);\n"
                            "        %1%_unpack(&%1%_incoming.back());\n"
                            "    }\n"
                            )
                    % op->name();
        os << "\n";
    }
    os << "};\n";

    os << "void " << mod->name() << "::prepareInputs() {\n";
    for (InputPort* ip: mod->inputs()) {
        auto sig = typeSig(ip->type(), false, -1);
        os << boost::format(
            "    if (!%1%_outgoing.empty()) {\n"
            "        %1%_pack(%1%_outgoing.front());\n"
            "        simulator->%1%_valid = 1;\n"
            "    } else {\n"
            "        simulator->%1%_valid = 0;\n"
            "    }\n")
            % ip->name();
        os << "\n";
    }
    os << "};\n";

    os << "void " << mod->name() << "::writeInputs() {\n";
    for (InputPort* ip: mod->inputs()) {
        auto sig = typeSig(ip->type(), false, -1);
        os << boost::format(
            "    if (simulator->%1%_valid && !simulator->%1%_bp) {\n"
            "        %1%_outgoing.pop_front();\n"
            "    }\n")
            % ip->name();
        os << "\n";
    }
    os << "};\n";

    os << "void " << mod->name() << "::reset() {"
       << R"STRING(
    simulator->resetn = 0;
    this->run(1); // Five cycle reset -- totally arbitrary

    simulator->resetn = 1;
)STRING";
    for (auto op: mod->outputs()) {
        os << "    simulator->" << op->name() << "_bp = 1;\n";
    }
    os << R"STRING(
    this->run(1); // Five cycles after reset -- totally arbitrary
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
        string tName = ip->name() + "_type";
        os << boost::format("void %3%::%1%_pack(%2% arg) {\n")
                    % ip->name()
                    % tName
                    % mod->name();

        if (bitwidth(ip->type()) > 0) {
            os << boost::format(
                    "    memcpy(%2%simulator->%1%, arg.arr, sizeof(arg));\n")
                    % ip->name()
                    % (bitwidth(ip->type()) <= 64 ? "&" : "") ;
        }

        os << "}\n\n";

        os << boost::format(
            "void %3%::%1%_nonblock(%2% arg) {\n"
            "    %1%_outgoing.push_back(arg);\n"
            "}\n")
            % ip->name()
            % tName
            % mod->name();

        os << boost::format(
            "void %3%::%1%(%2% arg) {\n"
            "    %1%_nonblock(arg);\n"
            "    while (!%1%_outgoing.empty()) {\n"
            "        run(1);\n"
            "    }\n"
            "}\n")
            % ip->name()
            % tName
            % mod->name();

        auto sig = typeSig(ip->type(), false, 0);
        os << boost::format(
                "void %3%::%1%(%2%    ) {\n"
                "    %4% arg;\n")
                % ip->name()
                % sig
                % mod->name()
                % tName;

        auto type = ip->type();
        auto args = numArgs(type);
        if (!type->isVoidTy()) {
            for (unsigned arg=0; arg<args; arg++) {
                auto argType = nthType(type, arg);
                if (implicitPointer(argType))
                    os << boost::format(
                        "    memcpy(arg.%1%, %1%, %2%);\n")
                            % argName(argType, arg)
                            % ((bitwidth(argType) + 7) / 8);
                else if (bitwidth(argType) > 0)
                    os << "    arg." << argName(argType, arg) << " = "
                       << argName(argType, arg) << ";\n";
            }
        }

        os << boost::format(
                "    %1%(arg);\n"
                "}\n")
                % ip->name();
    }

    // Code for unpacking outputs
    for (OutputPort* op: mod->outputs()) {
        os << "// Input port '" << op->name() << "'\n";
        string tName = op->name() + "_type";

        if (bitwidth(op->type()) > 0) {
            os << boost::format(
                    "void %3%::%1%_unpack(%2%* arg) {\n"
                    "    memcpy(arg, %4%simulator->%1%, sizeof(*arg));\n"
                    "}\n")
                        % op->name()
                        % tName 
                        % mod->name()
                        % (bitwidth(op->type()) <= 64 ? "&" : "") ;
        } else {
             os << boost::format(
                    "void %3%::%1%_unpack(%2%* arg) {\n"
                    "}\n")
                        % op->name()
                        % tName 
                        % mod->name();
        }

        os << boost::format("bool %3%::%1%_nonblock(%2%* arg) {\n"
                            "    if (%1%_incoming.size() == 0)\n"
                            "        return false;\n"
                            "    *arg = %1%_incoming.front();\n"
                            "    %1%_incoming.pop_front();\n"
                            "    return true;\n"
                            "}\n")
                    % op->name()
                    % tName
                    % mod->name();

        os << boost::format(
                "void %3%::%1%(%2%* arg) {\n"
                "    while(!%1%_nonblock(arg))\n"
                "        run(1);\n"
                "}\n")
                % op->name()
                % tName
                % mod->name();

        auto sig = typeSig(op->type(), true, 0);
        os << boost::format(
                "void %3%::%1%(%2%    ) {\n"
                "    %4% arg;\n"
                "    %1%(&arg);\n")
                % op->name()
                % sig
                % mod->name()
                % tName;

        auto type = op->type();
        auto args = numArgs(type);

        if (!type->isVoidTy()) {
            for (unsigned arg=0; arg<args; arg++) {
                auto argType = nthType(type, arg);
                if (implicitPointer(argType))
                    os << boost::format(
                        "    memcpy(%1%, arg.%1%, %2%);\n")
                            % argName(argType, arg)
                            % ((bitwidth(argType) + 7) / 8);
                else if (bitwidth(argType) > 0)
                    os << "    *" << argName(argType, arg) << " = "
                       << "arg." << argName(argType, arg) << ";\n";
            }
        }

        os << "}\n";
    }

    for (Interface* iface: mod->interfaces()) {
        if (!iface->server())
            continue;
        string retType = structName(iface->resp(), mod->name() + "::");
        string args = typeSig(iface->req()->type(), false, 0);
        string argNameList = "";
        unsigned numargs = numArgs(iface->req()->type());
        for (unsigned i=0; i<numargs; i++) {
            if (argType(i, iface->req()->type())->isVoidTy())
                continue;
            argNameList += str(boost::format("arg%1%") % i);
            if (i != (numargs-1))
                argNameList += ", ";
        }
        os << boost::format(
            "%3% %1%::%2%(%4%) {\n"
            "    %3% ret;\n"
            "    %5%(%6%);\n"
            "    %7%(&ret);\n"
            "    return ret;\n"
            "}\n" )
            % mod->name()
            % iface->name()
            % retType
            % args
            % iface->req()->name()
            % argNameList
            % iface->resp()->name();
    }

    os << "// Write testing stubs to be later replaced\n";
    for (llvm::Function* test: mod->tests()) {
        auto p = typeSigPlain(test->getReturnType(), false);
        string rt = p.first + p.second;
        os << "    "
           << "bool " << mod->name() << "::"
           << cpp_demangle(test->getName().str().c_str()) 
           << "\n{ return false; }\n";
    }

    os << "\n";
    f->close();
}

void VerilatorWedge::writeBodies(Module* mod) {
    // Write bodies for the interface functions.
   
    // Find function declarations for Module::test*
    llvm::Module* swmod = mod->swModule();
    string prefix = mod->name() + "::";
    map<string, llvm::Function*> memberMap;
    map<string, llvm::Function*> interfaceMap;
    for (llvm::Function& func: swmod->getFunctionList()) {
        string name = cpp_demangle(func.getName().str().c_str());
        if (name.find(prefix) == 0) {
            // It's a member
            string shortName = name.substr(prefix.size());
            // cout << shortName << endl;
            memberMap[shortName] = &func;

            string ifaceName =
                shortName.substr(0, shortName.find_first_of("(*:"));
            interfaceMap[ifaceName] = &func;
            // cout << ifaceName << endl;
        }
    }

    // For each interface stub (a C function), have it call the C++
    // version.
    auto stubs = mod->interfaceStubs();
    for (auto p: stubs) {
        llvm::Function* testThunk = p.second;
        llvm::Function* ifaceFunc = interfaceMap[p.first];
        assert(ifaceFunc != NULL);
        auto& ctxt = mod->design().context();
        auto bb = llvm::BasicBlock::Create(ctxt, "thunk", testThunk);
        vector<llvm::Value*> args;
        for (auto arg = testThunk->arg_begin();
                arg != testThunk->arg_end(); arg++) {
            args.push_back(&*arg);
        }
        auto call = llvm::CallInst::Create(ifaceFunc, args, "", bb);
        llvm::ReturnInst::Create(ctxt, call, bb);
    }

    // Add tests which automatically run both S/W and H/W then compare
    // the results
    for (llvm::Function* test: mod->tests()) {
        llvm::Function* memberFunc =
            memberMap[cpp_demangle(test->getName().str().c_str())];
        if (memberFunc == NULL) {
            printf("Warn: could not find member stub for %s\n",
                   test->getName().str().c_str());
            continue;
        }

        memberFunc->deleteBody();

        llvm::ValueToValueMapTy vmap;
        llvm::Function::arg_iterator mfIter = memberFunc->arg_begin();
        llvm::Function::arg_iterator tfIter = test->arg_begin();
        mfIter++;
        while (mfIter != memberFunc->arg_end() &&
               tfIter != test->arg_end()) {
            vmap[&*mfIter] = &*tfIter;
            mfIter++;
            tfIter++;
        }
        assert(mfIter == memberFunc->arg_end() && tfIter == test->arg_end());

        llvm::SmallVector<llvm::ReturnInst*, 8> returns;
        llvm::CloneFunctionInto(
            memberFunc, test, vmap, true, returns);
        addAssertsToTest(mod, memberFunc);
    }
}

void VerilatorWedge::addAssertsToTest(Module* mod, llvm::Function* test) {
    string prefix = mod->name() + "::";
    for (auto& bb: test->getBasicBlockList()) {
        for (auto& ins: bb.getInstList()) {
            if (ins.getOpcode() == llvm::Instruction::Call) {
                llvm::CallInst* ci = llvm::dyn_cast<llvm::CallInst>(&ins);
                string rawFuncName =
                    ci->getCalledFunction()->getName().str();
                if (rawFuncName.rfind("_sw") == (rawFuncName.size() - 3)) {
                    string funcName = cpp_demangle(
                        rawFuncName.substr(0, rawFuncName.size() - 3));
                    if (funcName.find(prefix) == 0) {
                        string ifaceName = funcName.substr(prefix.size());
                        ifaceName = ifaceName.substr(0, ifaceName.find("("));
                        addAssertToTest(mod, ifaceName, test, ci);
                    }
                }
            }
        }
    }
}

llvm::Value* buildEqualityTest(llvm::Value* a, llvm::Value* b,
                               llvm::Instruction* insertAfter) {
    llvm::Instruction* insertBefore = insertAfter->getNextNode();
    llvm::Type* eqType = a->getType();
    assert(eqType == b->getType());
    switch(eqType->getTypeID()) {
    case llvm::Type::VoidTyID:
        return llvm::ConstantInt::getTrue(a->getContext());
    case llvm::Type::IntegerTyID:
        return llvm::ICmpInst::Create(
            llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ,
            a, b, "", insertBefore);
    case llvm::Type::FloatTyID:
        return llvm::ICmpInst::Create(
            llvm::Instruction::FCmp, llvm::CmpInst::ICMP_EQ,
            a, b, "", insertBefore);
    default:
        assert(false && "Not built yet!");
    }
}

void VerilatorWedge::addAssertToTest(Module* mod,
                                     string ifaceName,
                                     llvm::Function* func,
                                     llvm::CallInst* ci) {
    llvm::Type* swType = mod->swType();
    llvm::Argument* thisPtr = &*func->arg_begin();
    assert(thisPtr->getType() == mod->ifaceType()->getPointerTo(0));

    vector<llvm::Value*> operands;
    for (unsigned i=0; i<ci->getNumArgOperands(); i++) {
        auto operand = ci->getArgOperand(i);
        if (i == 0) {
            assert(operand->getType() == swType->getPointerTo(0));
            operands.push_back(thisPtr);
        } else {
            operands.push_back(operand);
        }
    }

    auto hwFunc = mod->interfaceStub(ifaceName);
    assert(hwFunc != NULL);
    auto hwCall = llvm::CallInst::Create(
        hwFunc, operands, "hwCall");
    hwCall->insertAfter(ci);
    llvm::Instruction* tail = hwCall->getNextNode();

    // Build code to test
    llvm::Value* isEqual = buildEqualityTest(ci, hwCall, hwCall);

    llvm::TerminatorInst* thenTerm;
    llvm::TerminatorInst* elseTerm;
    llvm::SplitBlockAndInsertIfThenElse(
        isEqual, tail, &thenTerm, &elseTerm);

    auto& ctxt = mod->design().context();
    llvm::ReturnInst::Create(
        ctxt,
        llvm::ConstantInt::getFalse(ctxt),
        elseTerm);
    elseTerm->removeFromParent();
}

} // namespace llpm

