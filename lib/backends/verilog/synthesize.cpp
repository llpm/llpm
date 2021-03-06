#include "synthesize.hpp"

#include <cmath>

#include <libraries/synthesis/pipeline.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>
#include <refinery/refinery.hpp>

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/std_library.hpp>
#include <libraries/synthesis/memory.hpp>
#include <libraries/synthesis/fork.hpp>
#include <libraries/legacy/rtl_wrappers.hpp>

#include <llvm/IR/Constants.h>

#include <boost/format.hpp>

using namespace std;

namespace llpm {

// Verilog libraries
static const vector<string> externalFiles {
    "/support/backends/verilog/select.sv",
    "/support/backends/verilog/idx_select.sv",
    "/support/backends/verilog/pipeline.sv",
    "/support/backends/verilog/memory.sv",
    "/support/backends/verilog/fork.sv",
};

static const vector<string> svKeywords {
    "alias",
    "always",
    "always_comb",
    "always_ff",
    "always_latch",
    "and",
    "assert",
    "assign",
    "assume",
    "automatic",
    "before",
    "begin",
    "bind",
    "bins",
    "binsof",
    "bit",
    "break",
    "buf",
    "bufif0",
    "bufif1",
    "byte",
    "case",
    "casex",
    "casez",
    "cell",
    "chandle",
    "class",
    "clocking",
    "cmos",
    "config",
    "const",
    "constraint",
    "context",
    "continue",
    "cover",
    "covergroup",
    "coverpoint",
    "cross",
    "deassign",
    "default",
    "defparam",
    "design",
    "disable",
    "dist",
    "do",
    "edge",
    "else",
    "end",
    "endcase",
    "endclass",
    "endclocking",
    "endconfig",
    "endfunction",
    "endgenerate",
    "endgroup",
    "endinterface",
    "endmodule",
    "endpackage",
    "endprimitive",
    "endprogram",
    "endproperty",
    "endspecify",
    "endsequence",
    "endtable",
    "endtask",
    "enum",
    "event",
    "expect",
    "export",
    "extends",
    "extern",
    "final",
    "first_match",
    "for",
    "force",
    "foreach",
    "forever",
    "fork",
    "forkjoin",
    "function",
    "generate",
    "genvar",
    "highz0",
    "highz1",
    "if",
    "iff",
    "ifnone",
    "ignore_bins",
    "illegal_bins",
    "import",
    "incdir",
    "include",
    "initial",
    "inout",
    "input",
    "inside",
    "instance",
    "int",
    "integer",
    "interface",
    "intersect",
    "join",
    "join_any",
    "join_none",
    "large",
    "liblist",
    "library",
    "local",
    "localparam",
    "logic",
    "longint",
    "macromodule",
    "matches",
    "medium",
    "modport",
    "module",
    "nand",
    "negedge",
    "new",
    "nmos",
    "nor",
    "noshowcancelled",
    "not",
    "notif0",
    "notif1",
    "null",
    "or",
    "output",
    "package",
    "packed",
    "parameter",
    "pmos",
    "posedge",
    "primitive",
    "priority",
    "program",
    "property",
    "protected",
    "pull0",
    "pull1",
    "pulldown",
    "pullup",
    "pulsestyle_onevent",
    "pulsestyle_ondetect",
    "pure",
    "rand",
    "randc",
    "randcase",
    "randsequence",
    "rcmos",
    "real",
    "realtime",
    "ref",
    "reg",
    "release",
    "repeat",
    "return",
    "rnmos",
    "rpmos",
    "rtran",
    "rtranif0",
    "rtranif1",
    "scalared",
    "sequence",
    "shortint",
    "shortreal",
    "showcancelled",
    "signed",
    "small",
    "solve",
    "specify",
    "specparam",
    "static",
    "string",
    "strong0",
    "strong1",
    "struct",
    "super",
    "supply0",
    "supply1",
    "table",
    "tagged",
    "task",
    "this",
    "throughout",
    "time",
    "timeprecision",
    "timeunit",
    "tran",
    "tranif0",
    "tranif1",
    "tri",
    "tri0",
    "tri1",
    "triand",
    "trior",
    "trireg",
    "type",
    "typedef",
    "union",
    "unique",
    "unsigned",
    "use",
    "var",
    "vectored",
    "virtual",
    "void",
    "wait",
    "wait_order",
    "wand",
    "weak0",
    "weak1",
    "while",
    "wildcard",
    "wire",
    "with",
    "within",
    "wor",
    "xnor",
    "xor"
};

VerilogSynthesizer::VerilogSynthesizer(Design& d) :
    Backend(d)
{
    StdLibStops(_stops);
    addStops();
    addDefaultPrinters();

    d.namer().reserveName("clk", nullptr);
    d.namer().reserveName("resetn", nullptr);
    for (auto svKeyword: svKeywords) {
        d.namer().reserveName(svKeyword, nullptr);
    }
}

void VerilogSynthesizer::addStops() {
    _stops.addClass<BlockRAM>();
    _stops.addClass<RTLReg>();
    _stops.addClass<Latch>();
}

#if 0
void VerilogSynthesizer::write(std::ostream& os) {
    auto& modules = _design.modules();
    for (auto& m: modules) {
        writeModule(os, m);
    }
}
#endif

InputPort* VerilogSynthesizer::findSink(const ConnectionDB* conns,
                                        const OutputPort* op) {
    vector<InputPort*> ports;
    conns->findSinks(op, ports);
    if (ports.size() == 0)
        return NULL;
    if (ports.size() == 1)
        return ports.front();
    throw InvalidCall("Synthesis does not support fan out greater than 1! "
                      "Run a pass to create forking blocks first");
}

static const std::string header = R"STRING(
/*****************
 *  This code autogenerated by LLPM.
 *  DO NOT MODIFY MANUALLY!
 *  Manual changes will be overwritten.
 ******/
`default_nettype none
)STRING";

#if 0
void VerilogSynthesizer::writeModule(FileSet::File* f, Module* mod) {
    writeModule(f->openStream(), mod);
    f->close();
}
#endif

void VerilogSynthesizer::writeModule(FileSet& dir,
                                     Module* mod,
                                     std::set<FileSet::File*>& files) 
{
    if (mod->is<ControlRegion>()) {
        // Force a schedule if not yet done.
        mod->as<ControlRegion>()->clocks();
    }

    for (auto f: externalFiles) {
        auto cpy = dir.copy(Directories::llpmLibraryPath() + f);
        files.insert(cpy);
    }

    if (mod->is<WrapLLPMMModule>()) {
        writeWrapper(dir, mod->as<WrapLLPMMModule>(), files);
        return;
    }

    auto vf = dir.create(mod->name() + ".sv");
    files.insert(vf);
    std::ostream& os = vf->openStream();

    Context ctxt(os, mod);


    ctxt << header;
    ctxt << "\n\n";
    ctxt << "// The \"" << mod->name() << "\" module of type "
         << cpp_demangle(typeid(*mod).name()) << "\n";
    ctxt << "module " << mod->name() << " (\n";

    writeIO(ctxt);
    writeBlocks(ctxt);

#if 0
    bool globalBP = ctxt.module()->is<ControlRegion>();
    if (globalBP) {
        writeCRControl(ctxt);
    } else {
        writeLocalIOControl(ctxt);
    }

#endif
    ctxt << "endmodule\n";
    ctxt << "`default_nettype wire\n";


    vector<Module*> submodules;
    mod->submodules(submodules);
    for (Module* sm: submodules) {
        ControlRegion* cr = dynamic_cast<ControlRegion*>(sm);
        if (cr != NULL) {
            ctxt << "\n";
            writeModule(dir, cr, files);
        }
    }

    vf->close();
}

void VerilogSynthesizer::writeIO(Context& ctxt) {
    Module* mod = ctxt.module();

    for (auto&& ip: mod->inputs()) {
        auto inputName = ctxt.name(ip, true);
        auto width = bitwidth(ip->type());
        if (bitwidth(ip->type()) > 0)
            ctxt << boost::format("    input wire [%1%:0] %2%, //Type: %3%\n")
                    % (width - 1)
                    % inputName
                    % typestr(ip->type());
        ctxt << boost::format("    input wire %1%_valid,\n") % inputName;
        ctxt << boost::format("    output wire %1%_bp,\n") % inputName;

    }
    ctxt << "\n";
    for (auto&& op: mod->outputs()) {
        auto outputName = ctxt.name(op, true);
        auto width = bitwidth(op->type());
        if (bitwidth(op->type()) > 0)
            ctxt << boost::format("    output wire [%1%:0] %2%, //Type: %3%\n")
                    % (width - 1)
                    % outputName
                    % typestr(op->type());
        ctxt << boost::format("    output wire %1%_valid,\n") % outputName;
        ctxt << boost::format("    input wire %1%_bp,\n") % outputName;
    }

    ctxt << "\n"
         << "    input wire clk,\n"
         << "    input wire resetn\n"
         << ");\n\n";


}

void VerilogSynthesizer::writeCRControl(Context& ctxt) {
    Module* mod = ctxt.module();
    ControlRegion* cr = mod->as<ControlRegion>();
    assert(cr != nullptr);
    ConnectionDB* conns = mod->conns();
    if (conns == NULL) {
        throw InvalidArgument("Verilog synthesizer cannot operate on "
                              "opaque modules!");
    }

    InputPort* outControlSink = nullptr;
    OutputPort* inControlDriver = nullptr;
    OutputPort* outControl = nullptr;
    InputPort* inControl = nullptr;
    if (cr->clocks() > 0) {
        auto psc0 = cr->stageControllers(0);
        inControl = psc0->vin();
        inControlDriver = conns->findSource(psc0->vin());
        auto pscF = cr->stageControllers(cr->stageControllers_size()-1);
        vector<InputPort*> outPorts;
        conns->findSinks(pscF->vout(), outPorts);
        if (outPorts.size() != 1) {
            throw InvalidArgument("All PipelineStageControllers must drive one "
                                  "and only one input from vout!");
        }
        outControl = pscF->vout();
        outControlSink = outPorts.front();
    }

    ctxt << "    wire cr_valid = \n";
    for (auto ip: mod->inputs()) {
        ctxt << "        " << ctxt.name(ip, true) << "_valid & \n";
    }
    ctxt << "        1'b1;\n\n";

    if (inControlDriver) {
        ctxt << boost::format("    wire %1%_valid = cr_valid;\n")
                % ctxt.name(inControlDriver);
    } 

    ctxt << "    wire cr_bp = \n";
    if (cr->clocks() == 0) {
         ctxt << "        ~cr_valid | \n";
    }
    for (auto op: mod->outputs()) {
        ctxt << "        " << ctxt.name(op, true) << "_bp | \n";
    }
    ctxt << "        1'b0;\n\n";

    for (auto&& ip: mod->inputs()) {
        OutputPort* dummyOP = mod->getDriver(ip);
        ctxt.namer().assignName(dummyOP, mod, ctxt.name(ip, true));
        if (inControl) {
            ctxt << boost::format("    assign %1%_bp = %2%_bp;\n")
                            % ctxt.name(ip, true)
                            % ctxt.name(inControl);
        } else {
            ctxt << boost::format("    assign %1%_bp = cr_bp;\n")
                            % ctxt.name(ip, true);
        }
    }

    if (outControlSink) {
        ctxt << boost::format("    wire %1%_bp = cr_bp;\n")
                % ctxt.name(outControlSink);
    }

    for (auto&& op: mod->outputs()) {
        InputPort* dummyIP = mod->getSink(op);
        OutputPort* source = conns->findSource(dummyIP);
        ctxt.namer().assignName(dummyIP, mod, ctxt.name(op, true));
        if (bitwidth(op->type()) > 0)
            ctxt << boost::format("    assign %1% = %2%;\n")
                        % ctxt.name(op, true)
                        % ctxt.name(source);

        if (outControl) {
            ctxt << boost::format("    assign %1%_valid = %2%_valid;\n")
                            % ctxt.name(op, true)
                            % ctxt.name(outControl);
        } else {
            ctxt << boost::format("    assign %1%_valid = cr_valid;\n")
                            % ctxt.name(op, true);
        }
    }

    ctxt << "\n";
}

void VerilogSynthesizer::writeLocalIOControl(Context& ctxt) {
    Module* mod = ctxt.module();
    ConnectionDB* conns = mod->conns();
    if (conns == NULL) {
        throw InvalidArgument("Verilog synthesizer cannot operate on "
                              "opaque modules!");
    }

    ctxt << "    // Input back pressure connection\n";
    for (auto&& ip: mod->inputs()) {
        OutputPort* dummyOP = mod->getDriver(ip);

        ctxt.namer().assignName(dummyOP, mod, ctxt.name(ip, true));

        InputPort* sink = findSink(conns, dummyOP);
        if (sink) {
            ctxt << boost::format("    assign %1%_bp = %2%_bp;\n")
                        % ctxt.name(ip, true)
                        % ctxt.name(sink);
        } else {
            ctxt << boost::format("    assign %1%_bp = 1'b0;\n")
                        % ctxt.name(ip, true);
        }
    }

    for (auto&& op: mod->outputs()) {
        InputPort* dummyIP = mod->getSink(op);
        ctxt.namer().assignName(dummyIP, mod, ctxt.name(op, true));
        OutputPort* source = conns->findSource(dummyIP);
        if (source != nullptr) {
            if (bitwidth(op->type()) > 0)
                ctxt << boost::format("    assign %1% = %2%;\n")
                            % ctxt.name(op, true)
                            % ctxt.name(source);
            ctxt << boost::format("    assign %1%_valid = %2%_valid;\n")
                        % ctxt.name(op, true)
                        % ctxt.name(source);
        }
    }

    ctxt << "\n";
}

void VerilogSynthesizer::writeBlocks(Context& ctxt) {
    ConnectionDB* conns = ctxt.module()->conns();
    Module* mod = ctxt.module();
    
    set<Block*> blocks;
    conns->findAllBlocks(blocks);

    for (auto ip: mod->inputs()) {
        blocks.insert(mod->getDriver(ip)->owner());
    }
    for (auto op: mod->outputs()) {
        blocks.insert(mod->getSink(op)->owner());
    }

    // Track block names for sanity checking
    std::map<std::string, Block*> blockNames;

    ctxt << "/*********\n"
         << "   Signal forward defs\n"
         << " *********/\n"
         << "\n";
        
    for (Block* b: blocks) {
        string bname = ctxt.primBlockName(b);
        auto bnF = blockNames.find(bname);
        assert(bnF == blockNames.end() ||
               bnF->second == b);
        blockNames[bname] = b;

        const vector<Printer*>& possible_printers = _printers(b);
        bool writeControlBits = !ctxt.module()->is<ControlRegion>();
        if (possible_printers.size() == 0) {
            auto blockName = ctxt.name(b);
            throw ImplementationError(
                str(boost::format(
                        " Cannot translate block %1% of type %2% into verilog.") 
                            % blockName
                            % cpp_demangle(typeid(*b).name())));
        }
        auto printer = possible_printers.front();

        ctxt << "    // Signal fwd defs \"" << ctxt.primBlockName(b) << "\" type "
             << cpp_demangle(typeid(*b).name()) << "\n";
        for (InputPort* ip: b->inputs()) {
            if (bitwidth(ip->type()) > 0) {
                ctxt << boost::format("    wire [%1%-1:0] %2%;\n")
                            % bitwidth(ip->type())
                            % ctxt.name(ip);
            }
            if (writeControlBits || printer->alwaysWriteValid(ip)) {
                ctxt << boost::format("    wire %1%_valid;\n")
                            % ctxt.name(ip);
            }
            if (writeControlBits || printer->alwaysWriteBP(ip)) {
                ctxt << boost::format("    wire %1%_bp;\n")
                            % ctxt.name(ip);
            } 
        }

        for (OutputPort* op: b->outputs()) {
            if (bitwidth(op->type()) > 0) {
                ctxt << boost::format("    wire [%1%-1:0] %2%;\n")
                            % bitwidth(op->type())
                            % ctxt.name(op);
            }

            if (writeControlBits || printer->alwaysWriteValid(op)) {
                ctxt << boost::format("    wire %1%_valid;\n")
                                      % ctxt.name(op);
            }
            if (writeControlBits || printer->alwaysWriteBP(op)) {
                ctxt << boost::format("    wire %1%_bp;\n")
                            % ctxt.name(op);
            }
        }

        ctxt << "\n";
    }

    ctxt << "/*********\n"
         << "   Module I/O Connections\n"
         << " *********/\n"
         << "\n";

    bool globalBP = ctxt.module()->is<ControlRegion>();
    if (globalBP) {
        writeCRControl(ctxt);
    } else {
        writeLocalIOControl(ctxt);
    }

    ctxt << "/*********\n"
         << "   Block implementations\n"
         << " *********/\n"
         << "\n";

    for (Block* b: blocks) {
        if (dynamic_cast<DummyBlock*>(b) != NULL)
            continue;

        const vector<Printer*>& possible_printers = _printers(b);
        bool writeControlBits = !ctxt.module()->is<ControlRegion>();
        if (possible_printers.size() == 0) {
            auto blockName = ctxt.name(b);
            throw ImplementationError(
                str(boost::format(
                        " Cannot translate block %1% of type %2% into verilog.") 
                            % blockName
                            % cpp_demangle(typeid(*b).name())));
        }
        auto printer = possible_printers.front();

        ctxt << "    // Block \"" << ctxt.primBlockName(b) << "\" type "
             << cpp_demangle(typeid(*b).name()) << "\n";
        for (InputPort* ip: b->inputs()) {
            Connection c;
            auto inpFound = conns->find(ip, c);
            std::string opName;
            if (!inpFound) {
                fprintf(stderr, "Warning: no driver found for input %s!\n",
                        ctxt.name(ip).c_str());
                fprintf(stderr, "         input %u of %lu of block %s type %s\n",
                                b->inputNum(ip)+1, b->inputs().size(),
                                ctxt.name(b).c_str(),
                                cpp_demangle(typeid(*b).name()).c_str());
                opName = "UNKNOWN";
            } else {
                opName = ctxt.name(c.source());
            }

            if (bitwidth(ip->type()) > 0) {
                ctxt << boost::format("    assign %1% = %2%;\n")
                            % ctxt.name(ip)
                            % opName;
            }
            if (writeControlBits || printer->alwaysWriteValid(ip)) {
                ctxt << boost::format("    assign %1%_valid = %2%_valid;\n")
                            % ctxt.name(ip)
                            % opName;
            }
        }

        for (OutputPort* op: b->outputs()) {
            if (writeControlBits || printer->alwaysWriteBP(op)) {
                ctxt << boost::format("    assign %1%_bp = ")
                            % ctxt.name(op);
                InputPort* sink = findSink(conns, op);
                if (sink) {
                    ctxt << ctxt.name(sink) << "_bp;\n";
                } else {
                    ctxt << "1'b0;\n";
                }
            }
        }

        print(ctxt, b);
        ctxt << "\n";
    }


}

void VerilogSynthesizer::print(Context& ctxt, Block* b)
{
    const vector<Printer*>& possible_printers = _printers(b);
    if (possible_printers.size() == 0) {
        auto blockName = ctxt.name(b);
        throw ImplementationError(
            str(boost::format(" Cannot translate block %1% of type %2% into verilog.") 
                            % blockName
                            % cpp_demangle(typeid(*b).name())));
    } else {
        auto printer = possible_printers.front();
        printer->print(ctxt, b);
        bool writeControlBits = !ctxt.module()->is<ControlRegion>();

        if (writeControlBits && !printer->customLID()) {
            assert(b->outputsTied());

            std::string joinOp;
            std::string terminator;
            switch (b->firing()) {
            case DependenceRule::AND_FireOne:
                joinOp = "&";
                terminator = "1'b1";
                break;
            case DependenceRule::Custom:
                assert(printer->customLID() &&
                       "This default printer can't deal with custom"
                       "firing rules!");
            }

            ctxt << "    wire " << ctxt.name(b) << "_valid = \n";
            for (auto ip: b->inputs()) {
                ctxt << "        " << ctxt.name(ip) << "_valid " << joinOp << "\n";
            }
            ctxt << "        " << terminator << ";\n";

            for (auto op: b->outputs()) {
                ctxt << "    assign " << ctxt.name(op) << "_valid = " << ctxt.name(b) << "_valid;\n";
            }


            ctxt << "    wire " << ctxt.name(b) << "_bp = \n";
            ctxt << "        ~" << ctxt.name(b) << "_valid | \n";
            for (auto op: b->outputs()) {
                ctxt << "        " << ctxt.name(op) << "_bp | " << "\n";
            }
            ctxt << "        1'b0;\n";

            for (auto ip: b->inputs()) {
                ctxt << "    assign " << ctxt.name(ip) << "_bp = " << ctxt.name(b) << "_bp;\n";
            }
        }
    }
}

void print_function(VerilogSynthesizer::Context& ctxt, Block* c,
                    const char* op, bool signedWrap) {
    Function* b = dynamic_cast<Function*>(c);
    ctxt << "    assign " << ctxt.name(b->dout()) << " = ";
    auto dinType = b->din()->type();
    assert(dinType->isStructTy());
    auto dinName = ctxt.name(b->din());
    bool first = true;
    for (unsigned i = 0; i < numContainedTypes(dinType); i++) {
        unsigned offset = bitoffset(dinType, i);
        unsigned width = bitwidth(nthType(dinType, i));
        if (width == 0)
            continue;
        if (first)
            first = false;
        else
            ctxt << " " << op << " ";
        if (signedWrap)
            ctxt << boost::format("$signed(%1%[%2%:%3%])") 
                            % dinName
                            % (offset+width-1)
                            % offset;
        else 
            ctxt << boost::format("%1%[%2%:%3%]") 
                            % dinName
                            % (offset+width-1)
                            % offset;
    }
    ctxt << ";\n";
}

template<typename BlockType>
class BinaryOpPrinter : public VerilogSynthesizer::Printer {
    const char* _op;
public:
    BinaryOpPrinter(const char* op) : _op(op) { }

    bool handles(Block* b) const {
        return dynamic_cast<BlockType*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        bool signedWrap = false;
        if (IntDivide* id = dynamic_cast<IntDivide*>(c))
            signedWrap = id->isSigned();
        if (IntRemainder* ir = dynamic_cast<IntRemainder*>(c))
            signedWrap = ir->isSigned();
        print_function(ctxt, c, _op, signedWrap);
    }
};

class CompareOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntCompare*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        const char* op = NULL;
        IntCompare* b = dynamic_cast<IntCompare*>(c);

        switch (b->op()) {
        case IntCompare::EQ:
            op = "==";
            break;
        case IntCompare::NEQ:
            op = "!=";
            break;
        case IntCompare::GT:
            op = ">";
            break;
        case IntCompare::GTE:
            op = ">=";
            break;
        }
        print_function(ctxt, c, op, b->isSigned());
    }
};

class BitwiseOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Bitwise*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        const char* op = NULL;
        Bitwise* b = dynamic_cast<Bitwise*>(c);
        switch (b->op()) {
        case Bitwise::AND:
            op = "&";
            break;
        case Bitwise::OR:
            op = "|";
            break;
        case Bitwise::XOR:
            op = "^";
            break;
        }
        print_function(ctxt, c, op, false);
    }
};

class ShiftPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Shift*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        const char* op = NULL;
        Shift* s = dynamic_cast<Shift*>(c);
        
        switch (s->dir()) {
        case Shift::Left:
            op = "<<";
            break;
        case Shift::Right:
            op = ">>";
            break;
        }

        switch (s->style()) {
        case Shift::Logical:
            print_function(ctxt, c, op, false);
            break;
        case Shift::Rotating: {
            assert(false && "Rotating shifts not yet implemented!");
            break;
        }
        case Shift::Arithmetic:
            assert(s->style() == Shift::Arithmetic);
            print_function(ctxt, c, ">>>", false);
            break;
        }
    }
};

class ConstShiftPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<ConstShift*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        ConstShift* s = dynamic_cast<ConstShift*>(c);

        unsigned width = bitwidth(s->dout()->type());
        int amt = s->shift();
        int absAmt;
        if (amt < 0)
            absAmt = -1 * amt;
        else
            absAmt= amt;

        string name = ctxt.name(s->din());
        ctxt << "    assign " << ctxt.name(s->dout()) << " = ";
        switch (s->style()) {
        case ConstShift::LogicalTruncating:
            if (s->shift() < 0) {
                ctxt << boost::format(
                        "%1%[%2%:%3%];\n")
                        % name
                        % (absAmt + width - 1)
                        % absAmt;
            } else {
                ctxt << boost::format(
                        "{%1%, {%2%{1'b0}}};\n")
                        % name
                        % absAmt;
            }
            break;
        case ConstShift::LogicalExtending: {
            assert(false && "Extending shifts not yet implemented!");
            break;
        }
        case ConstShift::Rotating: {
            assert(false && "Rotating shifts not yet implemented!");
            break;
        }
        case ConstShift::Arithmetic:
            ctxt << boost::format(
                    "%1% >>> %2%;\n")
                    % name
                    % absAmt;
            break;
        }
    }
};

class IntTruncatePrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntTruncate*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Function* f = dynamic_cast<Function*>(c);
        ctxt << "    assign " << ctxt.name(f->dout()) << " = "
             << boost::format("%1%[%2%:0];\n")
                            % ctxt.name(f->din())
                            % (bitwidth(f->dout()->type()) - 1);
    }
};

class IntExtendPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntExtend*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        IntExtend* f = dynamic_cast<IntExtend*>(c);
        unsigned ib = bitwidth(f->din()->type());
        unsigned ob = bitwidth(f->dout()->type());
        assert(ob > ib);
        string msb = str(boost::format("%1%[%2%]")
                            % ctxt.name(f->din())
                            % (ib - 1));
        ctxt << "    assign " << ctxt.name(f->dout()) << " = { "
             << boost::format("{%1%{%2%}}")
                % (ob - ib)
                % ( f->signExtend() ? msb : "1'b0")
             << ", " << ctxt.name(f->din()) << "};\n";
    }
};

template<typename Op>
class IdentityOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Op*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Op* f = dynamic_cast<Op*>(c);
        if (bitwidth(f->dout()->type()) > 0)
            ctxt << "    assign " << ctxt.name(f->dout()) << " = "
                 << ctxt.name(f->din()) << ";\n";
    }
};

class NeverPrinter: public VerilogSynthesizer::Printer {
public:
    virtual bool customLID() const {
        return true;
    }

    bool handles(Block* b) const {
        return dynamic_cast<Never*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Never* f = dynamic_cast<Never*>(c);
        if (bitwidth(f->dout()->type()) > 0)
            ctxt << boost::format(
                        "    assign %1% = {%2%{1'bx}};\n")
                            % ctxt.name(f->dout())
                            % bitwidth(f->dout()->type());
        ctxt << boost::format(
                    "    assign %1%_valid = 1'b0;\n")
                        % ctxt.name(f->dout());
    }
};

class NullSinkPrinter: public VerilogSynthesizer::Printer {
public:
    virtual bool customLID() const {
        return true;
    }

    bool handles(Block* b) const {
        return dynamic_cast<NullSink*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        NullSink* f = dynamic_cast<NullSink*>(c);
        ctxt << boost::format(
                    "    assign %1%_bp = 1'b0;\n")
                        % ctxt.name(f->din());
    }
};

class ConstantPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Constant*>(b) != NULL;
    }

    static std::string toString(llvm::Type* ty, llvm::Constant* lc) {
        if (ty == NULL)
            ty = lc->getType();

        if (lc != NULL &&
            lc->getValueID() == llvm::Value::UndefValueVal) {
            return str(boost::format("{%1%{1'bx}}") %
                            bitwidth(ty));
        };

        unsigned valueID;
        if (lc != NULL) {
            valueID = lc->getValueID();
        } else {
            switch(ty->getTypeID()) {
            case llvm::Type::IntegerTyID:
                valueID = llvm::Value::ConstantIntVal;
                break;
            case llvm::Type::PointerTyID:
                valueID = llvm::Value::ConstantPointerNullVal;
                break;
            case llvm::Type::VectorTyID:
                valueID = llvm::Value::ConstantAggregateZeroVal;
                break;
            default:
                valueID = -1;
            }
        }

        switch (valueID) {
            case llvm::Value::ConstantIntVal: {
                string num =
                    llvm::dyn_cast<llvm::ConstantInt>(lc)
                        ->getValue().toString(10, true);
                if (num[0] == '-') {
                    return str(boost::format("-%1%'d%2%") 
                                % bitwidth(ty)
                                % num.substr(1));
                } else {
                    return str(boost::format("%1%'d%2%") 
                                % bitwidth(ty)
                                % num);
                }
            }
            case llvm::Value::ConstantPointerNullVal:
                assert(lc == NULL || lc->isNullValue());
                return str(boost::format("%1%'h%2%") 
                            % bitwidth(ty)
                            % 0);
            case llvm::Value::ConstantAggregateZeroVal: 
                return str(boost::format("{%1%{1'b0}}")
                            % bitwidth(ty));
            case llvm::Value::ConstantDataVectorVal: {
                llvm::ConstantDataVector* cv =
                    llvm::dyn_cast<llvm::ConstantDataVector>(lc);
                assert(cv != NULL);
                std::string ret = "{";
                unsigned len = numContainedTypes(ty);
                for (unsigned i=len; i>0; i--) {
                    ret += toString(NULL, cv->getElementAsConstant(i-1)) + 
                            (i == 1 ? "" : ", ");
                }
                ret += "}";
                return ret;
            }
            case llvm::Value::ConstantStructVal:
            case llvm::Value::ConstantVectorVal: {
                std::string ret = "{";
                unsigned len = numContainedTypes(ty);
                for (unsigned i=len; i>0; i--) {
                    ret += toString(NULL, lc->getAggregateElement(i-1)) + 
                            (i == 1 ? "" : ", ");
                }
                ret += "}";
                return ret;
            }
            default:
                throw InvalidArgument("Constant type not yet supported");
        }
    }

    static std::string toString(Constant* c) {
        llvm::Constant* lc = c->value();
        return toString(c->dout()->type(), lc);
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Constant* f = dynamic_cast<Constant*>(c);
        if (bitwidth(f->dout()->type()) > 0)
            ctxt << "    assign " << ctxt.name(f->dout()) << " = "
                 << toString(f) << ";\n";
    }
};

class OncePrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Once*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    static std::string toString(Once* o) {
        llvm::Constant* lc = o->value();
        return ConstantPrinter::toString(o->dout()->type(), lc);
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Once* f = dynamic_cast<Once*>(c);
        if (bitwidth(f->dout()->type()) > 0)
            ctxt << "    assign " << ctxt.name(f->dout()) << " = "
                 << toString(f) << ";\n";
        ctxt << "    reg " << ctxt.name(f) << "_valid;\n";
        ctxt << "    assign " << ctxt.name(f->dout()) << "_valid = "
                              << ctxt.name(f) << "_valid;\n";
        ctxt << "    always@(posedge clk)\n"
             << "    begin\n"
             << "        if (~resetn)\n"
             << "        begin\n"
             << "            " << ctxt.name(f) << "_valid <= 1'b1;\n"
             << "        end\n"
             << "        else if (" << ctxt.name(f->dout()) << "_bp == 1'b0)\n"
             << "        begin\n"
             << "            " << ctxt.name(f) << "_valid <= 1'b0;\n"
             << "        end\n"
             << "    end\n"
             << "\n";

    }
};

class JoinPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Join*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Join* j = dynamic_cast<Join*>(c);
        if (bitwidth(j->dout()->type()) == 0)
            return;
        ctxt << "    assign " << ctxt.name(j->dout()) << " = { ";
        bool first = true;
        for (signed i = j->din_size() - 1; i >= 0; i--) {
            if (bitwidth(j->din(i)->type()) == 0)
                continue;
            if (first)
                first = false;
            else
                ctxt << ", ";
            ctxt << ctxt.name(j->din(i));
        }
        ctxt << " };\n";
    }
};

class SplitPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Split*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Split* s = dynamic_cast<Split*>(c);
        auto dinName = ctxt.name(s->din());
        for (unsigned i=0; i < s->dout_size(); i++) {
            if (bitwidth(s->dout(i)->type()) == 0)
                continue;
            unsigned offset = bitoffset(s->din()->type(), i);
            unsigned width = bitwidth(nthType(s->din()->type(), i));
            ctxt << "    assign " << ctxt.name(s->dout(i)) << " = "
                 << boost::format("%1%[%2%:%3%];\n") 
                        % dinName
                        % (offset+width-1)
                        % offset;
        }
    }
};

class ExtractPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Extract*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Extract* e = dynamic_cast<Extract*>(c);
        auto dinName = ctxt.name(e->din());
        unsigned offset = 0; 
        llvm::Type* type = e->din()->type();
        for (unsigned i: e->path()) {
            offset += bitoffset(type, i);
            type = nthType(type, i);
        }

        unsigned width = bitwidth(e->dout()->type());

        if (width > 0) {
            ctxt << "    assign " << ctxt.name(e->dout()) << " = "
                 << boost::format("%1%[%2%:%3%];\n") 
                        % dinName
                        % (offset+width-1)
                        % offset;
        }
    }
};

class MultiplexerPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Multiplexer*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Multiplexer* m = dynamic_cast<Multiplexer*>(c);
        auto doutName = ctxt.name(m->dout());
        auto dinType = m->din()->type();
        auto dinName = ctxt.name(m->din());
        unsigned nWidth = bitwidth(nthType(dinType, 0));
        unsigned nOffset = bitoffset(dinType, 0);

        ctxt << boost::format(
                "    wire [%7%-1:0] %1%_sel = %4%[%5%:%6%];\n"
                "    wire [%3%-1:0][%2%-1:0] %1%_ic;\n")
                % ctxt.name(m)
                % bitwidth(m->dout()->type())
                % (numContainedTypes(dinType) - 1)
                % dinName
                % (nWidth + nOffset - 1)
                % nOffset
                % nWidth;

        for (size_t i=1; i<numContainedTypes(dinType); i++) {
            auto offset = bitoffset(dinType, i);
            ctxt << boost::format("    assign %1%_ic[%2%] = %3%[%4%:%5%];\n")
                        % ctxt.name(m)
                        % (i-1)
                        % dinName
                        % (offset + bitwidth(nthType(dinType, i)) - 1)
                        % offset;
        }
        
        ctxt << boost::format(
                "    assign %1% = %2%_ic[%2%_sel];\n")
                % doutName
                % ctxt.name(m);
    }
};

class RouterPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Router*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Router* r = dynamic_cast<Router*>(c);
        auto dinName = ctxt.name(r->din());
        auto dinType = nthType(r->din()->type(), 1);
        auto dinOffset = bitoffset(r->din()->type(), 1);
        auto dinWidth = bitwidth(dinType);

        auto selType = nthType(r->din()->type(), 0);
        auto selWidth = bitwidth(selType);
        auto selOffset = bitoffset(r->din()->type(), 0);

        for (unsigned i=0; i<r->dout_size(); i++) {
            auto op = r->dout(i);
            if (dinWidth > 0) {
                ctxt << boost::format(
                    "    assign %8% = (%1%[%2%:%3%] == %4%) ? \n"
                    "        %1%[%5%:%6%] : {%7%{1'bx}};\n")
                    % dinName
                    % (selWidth + selOffset - 1)
                    % selOffset
                    % i
                    % (dinWidth + dinOffset - 1)
                    % dinOffset
                    % dinWidth
                    % ctxt.name(op);
            }
            ctxt << boost::format(
                "    assign %5%_valid = (%1%[%2%:%3%] == %4%) ? \n"
                "        %1%_valid : 1'b0;\n")
                % dinName
                % (selWidth + selOffset - 1)
                % selOffset
                % i
                % ctxt.name(op);
        }
        ctxt << "    assign " << dinName << "_bp = \n";
        for (unsigned i=0; i<r->dout_size(); i++) {
            auto op = r->dout(i);
            ctxt << boost::format("        (%1%_bp & %1%_valid) |\n")
                        % ctxt.name(op);
        }
        ctxt << "        1'b0;\n";
    }
};

class SelectPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Select*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Select* s = dynamic_cast<Select*>(c);
        std::string style = "LLPM_Select_Priority";
        if (bitwidth(s->dout()->type()) == 0) {
            style += "NoData";
        }

        assert(s->din_size() > 0);
 
        if (bitwidth(s->dout()->type()) > 0) {
            ctxt << boost::format("    wire [%1%:0] %2%_input_combined [%3%:0];\n") 
                                % (bitwidth(s->dout()->type()) - 1)
                                % ctxt.name(s)
                                % (s->din_size() - 1);
        }

        ctxt << boost::format("    wire %1%_valids[%2%:0];\n") 
                            % ctxt.name(s)
                            % (s->din_size() - 1)
             << boost::format("    wire %1%_bp[%2%:0];\n") 
                            % ctxt.name(s)
                            % (s->din_size() - 1)
            ;
        for (unsigned i=0; i<s->din_size(); i++) {
            if (bitwidth(s->dout()->type()) > 0) {
                ctxt << boost::format("    assign %1%_input_combined[%2%] = %3%;\n")
                                % ctxt.name(s)
                                % i
                                % ctxt.name(s->din(i));
            }
            ctxt << boost::format("    assign %1%_valids[%2%] = %3%_valid;\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
#if 0
                 << boost::format("    assign %3%_bp = %1%_bp[%2%];\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
#endif
                ;
        }

        ctxt << boost::format("    %1% # (\n") % style
             << boost::format("        .Width(%1%),\n") % bitwidth(s->dout()->type())
             << boost::format("        .NumInputs(%1%), \n") % s->din_size()
             << boost::format("        .CLog2NumInputs(%1%)\n") 
                        % std::max((unsigned)1, (unsigned)ceil(log2(s->din_size())))
             << boost::format("    ) %1% (\n") % ctxt.name(c)
             <<               "        .clk(clk),\n"
             <<               "        .resetn(resetn),\n";
            
        if (bitwidth(s->dout()->type()) > 0) { 
             ctxt << boost::format("        .x(%1%_input_combined), \n") % ctxt.name(s)
                  << boost::format("        .a(%1%), \n") % ctxt.name(s->dout());
        }
        ctxt << boost::format("        .x_valid(%1%_valids), \n") % ctxt.name(s)
             << boost::format("        .x_bp(%1%_bp), \n") % ctxt.name(s)

             << boost::format("        .a_valid(%1%_valid), \n") % ctxt.name(s->dout())
             << boost::format("        .a_bp(%1%_bp) \n") % ctxt.name(s->dout())
             <<               "    );\n"
             ;

        for (unsigned i=0; i<s->din_size(); i++) {

                 ctxt << boost::format("    assign %3%_bp = %1%_bp[%2%];\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
                ;
        }
    }
};

class IdxSelectPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IdxSelect*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        IdxSelect* s = dynamic_cast<IdxSelect*>(c);
        std::string style = "LLPM_IdxSelect";
        if (bitwidth(s->dout()->type()) == 0) {
            style += "NoData";
        }

        assert(s->din_size() > 0);
 
        if (bitwidth(s->dout()->type()) > 0) {
            ctxt << boost::format("    wire [%1%:0] %2%_input_combined [%3%:0];\n") 
                                % (bitwidth(s->dout()->type()) - 1)
                                % ctxt.name(s)
                                % (s->din_size() - 1);
        }

        ctxt << boost::format("    wire %1%_valids[%2%:0];\n") 
                            % ctxt.name(s)
                            % (s->din_size() - 1)
             << boost::format("    wire %1%_bp[%2%:0];\n") 
                            % ctxt.name(s)
                            % (s->din_size() - 1)
            ;
        for (unsigned i=0; i<s->din_size(); i++) {
            if (bitwidth(s->dout()->type()) > 0) {
                ctxt << boost::format("    assign %1%_input_combined[%2%] = %3%;\n")
                                % ctxt.name(s)
                                % i
                                % ctxt.name(s->din(i));
            }
            ctxt << boost::format("    assign %1%_valids[%2%] = %3%_valid;\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
#if 0
                 << boost::format("    assign %3%_bp = %1%_bp[%2%];\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
#endif
                ;
        }

        ctxt << boost::format("    %1% # (\n") % style
             << boost::format("        .Width(%1%),\n") % bitwidth(s->dout()->type())
             << boost::format("        .NumInputs(%1%), \n") % s->din_size()
             << boost::format("        .CLog2NumInputs(%1%)\n") 
                        % std::max((unsigned)1, (unsigned)ceil(log2(s->din_size())))
             << boost::format("    ) %1% (\n") % ctxt.name(c)
             <<               "        .clk(clk),\n"
             <<               "        .resetn(resetn),\n"
             << boost::format("        .idx(%1%),\n"
                              "        .idx_valid(%1%_valid),\n"
                              "        .idx_bp(%1%_bp),\n")
                        % ctxt.name(s->idx());

        if (bitwidth(s->dout()->type()) > 0) { 
             ctxt << boost::format("        .x(%1%_input_combined), \n") % ctxt.name(s)
                  << boost::format("        .a(%1%), \n") % ctxt.name(s->dout());
        }
        ctxt << boost::format("        .x_valid(%1%_valids), \n") % ctxt.name(s)
             << boost::format("        .x_bp(%1%_bp), \n") % ctxt.name(s)

             << boost::format("        .a_valid(%1%_valid), \n") % ctxt.name(s->dout())
             << boost::format("        .a_bp(%1%_bp) \n") % ctxt.name(s->dout())
             <<               "    );\n"
             ;

        for (unsigned i=0; i<s->din_size(); i++) {

                 ctxt << boost::format("    assign %3%_bp = %1%_bp[%2%];\n")
                            % ctxt.name(s)
                            % i
                            % ctxt.name(s->din(i))
                ;
        }
    }
};

class ForkPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Fork*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* b) const {
        Fork* f = dynamic_cast<Fork*>(b);
        std::string style;
        if (f->virt())
            style = "VirtFork";
        else 
            style = "Fork";
        if (bitwidth(f->din()->type()) == 0)
            style += "_VoidData";
    
        if (bitwidth(f->din()->type()) > 0) {
            ctxt << boost::format(
                    "    wire [%2%:0] %1%_dout;\n")
                        % ctxt.name(f)
                        % (bitwidth(f->din()->type()) - 1);
        }
        ctxt << boost::format(
                "    wire [%2%:0] %1%_dout_valids;\n"
                "    wire [%2%:0] %1%_dout_bp;\n") 
                    % ctxt.name(f)
                    % (f->dout_size() - 1);

        for (unsigned i=0; i<f->dout_size(); i++) {
            if (bitwidth(f->dout(i)->type()) > 0) {
                ctxt << boost::format(
                    "    assign %2% = %1%_dout;\n")
                        % ctxt.name(f)
                        % ctxt.name(f->dout(i));
            } 
            ctxt << boost::format(
                "    assign %3%_valid = %1%_dout_valids[%2%];\n"
                "    assign %1%_dout_bp[%2%] = %3%_bp;\n")
                    % ctxt.name(f)
                    % i
                    % ctxt.name(f->dout(i));
        }

        ctxt << boost::format("    %1% # (\n") % style
             << boost::format("        .Width(%1%),\n") 
                             % bitwidth(f->din()->type())
             << boost::format("        .NumOutputs(%1%)\n")
                             % f->dout_size()
             << boost::format("    ) %1% (\n") % ctxt.name(f)
             <<               "        .clk(clk),\n"
             <<               "        .resetn(resetn),\n";

        if (bitwidth(f->din()->type()) > 0) {
            ctxt << boost::format("        .din(%2%), \n"
                                  "        .dout(%1%_dout), \n")
                            % ctxt.name(f)
                            % ctxt.name(f->din());
        }
        ctxt << boost::format("        .din_valid(%2%_valid), \n"
                              "        .din_bp(%2%_bp), \n"
                              "        .dout_valid(%1%_dout_valids), \n"
                              "        .dout_bp(%1%_dout_bp) \n")
                             % ctxt.name(f)
                             % ctxt.name(f->din())
             <<               "    );\n";
    }
};

class RTLRegPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<RTLReg*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* b) const {
        RTLReg* rr = dynamic_cast<RTLReg*>(b);
        std::string style = "RTLReg";
        auto type = rr->type();
        auto typeBW = bitwidth(type);
    
        if (typeBW > 0) {
            ctxt << boost::format(
                    "    wire [%2%:0] %1%_dout;\n")
                        % ctxt.name(rr)
                        % (typeBW - 1);
        }

        if (bitwidth(rr->type()) > 0) {
            ctxt << boost::format("    wire [%1%:0] %2%_write_reqs[%3%:0];\n") 
                                % (bitwidth(rr->type()) - 1)
                                % ctxt.name(rr)
                                % (rr->write_size() - 1);
        }

        ctxt << boost::format("    wire %1%_write_req_valids[%2%:0];\n") 
                            % ctxt.name(rr)
                            % (rr->write_size() - 1)
             << boost::format("    wire %1%_write_req_bps[%2%:0];\n") 
                            % ctxt.name(rr)
                            % (rr->write_size() - 1);

        ctxt << boost::format("    wire %1%_write_resp_valids[%2%:0];\n") 
                            % ctxt.name(rr)
                            % (rr->write_size() - 1)
             << boost::format("    wire %1%_write_resp_bps[%2%:0];\n") 
                            % ctxt.name(rr)
                            % (rr->write_size() - 1);

        for (unsigned i=0; i<rr->read_size(); i++) {
            Interface* readif = rr->read(i);
            OutputPort* resp = readif->dout();
            InputPort*  req  = readif->din();
            if (bitwidth(rr->type()) > 0) {
                ctxt << boost::format(
                    "    assign %2% = %1%_dout;\n")
                        % ctxt.name(rr)
                        % ctxt.name(resp);
            } 
            ctxt << boost::format(
                "    assign %1%_valid = %2%_valid;\n"
                "    assign %2%_bp    = %1%_bp;\n")
                    % ctxt.name(resp)
                    % ctxt.name(req);
        }

        for (unsigned i=0; i<rr->write_size(); i++) {
            Interface* writeif = rr->write(i);
            OutputPort* resp = writeif->dout();
            InputPort*  req  = writeif->din();
            if (bitwidth(rr->type()) > 0) {
                ctxt << boost::format(
                    "    assign %1%_write_reqs[%3%] = %2%;\n")
                        % ctxt.name(rr)
                        % ctxt.name(req)
                        % i;
            } 
            ctxt << boost::format(
                "    assign %1%_write_req_valids[%3%] = %2%_valid;\n"
                "    assign %2%_bp = %1%_write_req_bps[%3%];\n")
                    % ctxt.name(rr)
                    % ctxt.name(req)
                    % i;
            ctxt << boost::format(
                "    assign %2%_valid = %1%_write_resp_valids[%3%];\n"
                "    assign %1%_write_resp_bps[%3%]     = %2%_bp;\n")
                    % ctxt.name(rr)
                    % ctxt.name(resp)
                    % i;
        }

        ctxt << boost::format("    %1% # (\n") % style
             << boost::format("        .Width(%1%),\n") 
                             % typeBW
             << boost::format("        .NumWrites(%1%),\n") 
                             % rr->write_size()
             << boost::format("        .CLog2NumWrites(%1%),\n") 
                        % std::max((unsigned)1, (unsigned)ceil(log2(rr->write_size())))
             << boost::format("        .Name(\"%1%\")\n")
                             % ctxt.name(rr, true)
             << boost::format("    ) %1% (\n") % ctxt.name(rr)
             <<               "        .clk(clk),\n"
             <<               "        .resetn(resetn),\n";

        if (typeBW) {
            ctxt << boost::format("        .write_reqs(%1%_write_reqs), \n"
                                  "        .read(%1%_dout), \n")
                            % ctxt.name(rr);
        }
        ctxt << boost::format("        .write_req_valids(%1%_write_req_valids), \n"
                              "        .write_req_bps(%1%_write_req_bps), \n"
                              "        .write_resp_valids(%1%_write_resp_valids),\n"
                              "        .write_resp_bps(%1%_write_resp_bps)\n")
                             % ctxt.name(rr)
             <<               "    );\n";
    }
};

struct AttributePrinter {
    template<typename T>
    void print(VerilogSynthesizer::Context& ctxt,
               string k, T t, bool last = false) {
        ctxt << boost::format("        .%1%(%2%)%3%\n")
                    % k
                    % t
                    % (last ? "" : ",");
    }
    bool alwaysWriteValid(Port*) const {
        return false;
    }
    bool alwaysWriteBP(Port*) const {
        return false;
    }
    bool hasAttr() const {
        return true;
    }
};

template<typename BType, typename Attrs>
class VModulePrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<BType*>(b) != NULL;
    }

    virtual bool customLID() const {
        return true;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* b) const {
        BType* mod = dynamic_cast<BType*>(b);
        assert(mod != NULL);

        bool writeControlBits = !ctxt.module()->is<ControlRegion>();

        Attrs a;
        if (a.hasAttr()) {
            ctxt << "    " << a.name(mod) << " # ( \n";
            a(ctxt, mod);
            ctxt << "    ) " << ctxt.name(mod) << " (\n";
        } else {
            ctxt << "    " << a.name(mod)
                 << " " << ctxt.name(mod) << " (\n";
        }

        for (InputPort* ip: mod->inputs()) {
            if (bitwidth(ip->type()) > 0)
                ctxt << boost::format("        .%1%(%2%),\n")
                        % ctxt.name(ip, true)
                        % ctxt.name(ip);
            if (writeControlBits || a.alwaysWriteValid(ip)) {
                ctxt << boost::format("        .%1%_valid(%2%_valid),\n")
                        % ctxt.name(ip, true)
                        % ctxt.name(ip);
            }
            if (writeControlBits || a.alwaysWriteBP(ip)) {
                ctxt << boost::format("        .%1%_bp(%2%_bp),\n")
                        % ctxt.name(ip, true)
                        % ctxt.name(ip);
            }
        }

        for (OutputPort* op: mod->outputs()) {
            if (bitwidth(op->type()) > 0)
                ctxt << boost::format("        .%1%(%2%),\n")
                        % ctxt.name(op, true)
                        % ctxt.name(op);

            if (writeControlBits || a.alwaysWriteValid(op)) {
                ctxt << boost::format("        .%1%_valid(%2%_valid),\n")
                        % ctxt.name(op, true)
                        % ctxt.name(op);
            }
            if (writeControlBits || a.alwaysWriteBP(op)) {
                ctxt << boost::format("        .%1%_bp(%2%_bp),\n")
                        % ctxt.name(op, true)
                        % ctxt.name(op);
            }
        }

        ctxt << "        .clk(clk),\n"
             << "        .resetn(resetn)\n";

        ctxt << "    );\n";
    }
    bool alwaysWriteValid(Port* p) const {
        Attrs a;
        return a.alwaysWriteValid(p);
    }
    bool alwaysWriteBP(Port* p) const {
        Attrs a;
        return a.alwaysWriteBP(p);
    }
};

struct ModuleAttr: public AttributePrinter {
    std::string name(Module* m) {
        return m->name();
    }
    void operator()(VerilogSynthesizer::Context&, Block*) { }
    bool hasAttr() const {
        return false;
    }
};

struct PipelineRegAttr: public AttributePrinter {
    std::string name(Block* m) {
        auto preg = m->as<PipelineRegister>();
        if (preg->enable() != nullptr) {
            return "PipelineReg_Slave";
        } else {
            if (bitwidth(preg->dout()->type()) == 0)
                return "PipelineReg_DoubleWidth_NoData";
            return "PipelineReg_DoubleWidth";
        }
    }

    void operator()(VerilogSynthesizer::Context& ctxt,
                    PipelineRegister* r) {
        print(ctxt, "Name", "\"" + ctxt.name(r, false) + "\"", false);
        print(ctxt, "Width", bitwidth(r->din()->type()), true);
    }

    bool alwaysWriteValid(Port* p) const {
        return p->name() == "ce";
    }
};

struct PSCRegAttr: public AttributePrinter {
    std::string name(Block* m) {
        auto psc = m->as<PipelineStageController>();
        assert(psc != nullptr);
        return "PipelineStageController";
    }
    void operator()(VerilogSynthesizer::Context&,
                    PipelineStageController*) {
    }
    bool alwaysWriteValid(Port*) const {
        return true;
    }
    bool alwaysWriteBP(Port* p) const {
        return p->name() != "ce";
    }
    bool hasAttr() const {
        return false;
    }
};

struct LatchAttr: public AttributePrinter {
    std::string name(Block* m) {
        auto latch = m->as<Latch>();
        if (bitwidth(latch->dout()->type()) == 0)
            return "Latch_NoData";
        return "Latch";
    }

    void operator()(VerilogSynthesizer::Context& ctxt,
                    Latch* l) {
        print(ctxt, "Name", "\"" + ctxt.name(l) + "\"" , false);
        print(ctxt, "Width", bitwidth(l->din()->type()), true);
    }
};

struct BlockRAMAttr: public AttributePrinter {
    std::string name(Block* b) {
        BlockRAM* bram = dynamic_cast<BlockRAM*>(b);
        return str(boost::format("BlockRAM_%1%RW") % bram->ports_size());
    }
    void operator()(VerilogSynthesizer::Context& ctxt, BlockRAM* b) {
        print(ctxt, "Width", bitwidth(b->type()), false);
        print(ctxt, "Depth", b->depth(), false);
        print(ctxt, "AddrWidth", idxwidth(b->depth()), true);
    }
};

void VerilogSynthesizer::addDefaultPrinters() {
    _printers.appendEntry(make_shared<BinaryOpPrinter<IntAddition>>("+"));
    _printers.appendEntry(make_shared<BinaryOpPrinter<IntSubtraction>>("-"));
    _printers.appendEntry(make_shared<BinaryOpPrinter<IntMultiply>>("*"));
    _printers.appendEntry(make_shared<BinaryOpPrinter<IntDivide>>("/"));
    _printers.appendEntry(make_shared<BinaryOpPrinter<IntRemainder>>("%"));

    _printers.appendEntry(make_shared<CompareOpPrinter>());
    _printers.appendEntry(make_shared<BitwiseOpPrinter>());
    _printers.appendEntry(make_shared<ShiftPrinter>());
    _printers.appendEntry(make_shared<ConstShiftPrinter>());

    _printers.appendEntry(make_shared<IntTruncatePrinter>());
    _printers.appendEntry(make_shared<IntExtendPrinter>());

    _printers.appendEntry(make_shared<IdentityOpPrinter<Identity>>());
    _printers.appendEntry(make_shared<IdentityOpPrinter<Cast>>());
    _printers.appendEntry(make_shared<IdentityOpPrinter<Wait>>());

    _printers.appendEntry(make_shared<ConstantPrinter>());
    _printers.appendEntry(make_shared<OncePrinter>());
    _printers.appendEntry(make_shared<NeverPrinter>());
    _printers.appendEntry(make_shared<NullSinkPrinter>());

    _printers.appendEntry(make_shared<JoinPrinter>());
    _printers.appendEntry(make_shared<SelectPrinter>());
    _printers.appendEntry(make_shared<IdxSelectPrinter>());
    _printers.appendEntry(make_shared<SplitPrinter>());
    _printers.appendEntry(make_shared<ExtractPrinter>());
    _printers.appendEntry(make_shared<ForkPrinter>());

    _printers.appendEntry(make_shared<MultiplexerPrinter>());
    _printers.appendEntry(make_shared<RouterPrinter>());

    _printers.appendEntry(make_shared<RTLRegPrinter>());
    _printers.appendEntry(make_shared<VModulePrinter<PipelineRegister,
                                                     PipelineRegAttr>>());
    _printers.appendEntry(make_shared<VModulePrinter<PipelineStageController,
                                                     PSCRegAttr>>());
    _printers.appendEntry(make_shared<VModulePrinter<BlockRAM,
                                                     BlockRAMAttr>>());
    _printers.appendEntry(make_shared<VModulePrinter<Latch, LatchAttr>>());
    _printers.appendEntry(make_shared<VModulePrinter<Module, ModuleAttr>>());
}

bool VerilogSynthesizer::blockIsPrimitive(Block* b) {
    return primitiveStops()->stopRefine(b);
}

void VerilogSynthesizer::writeWrapper(
        FileSet& dir,
        WrapLLPMMModule* mod,
        std::set<FileSet::File*>& files)
{
    FileSet::File* vf = dir.create(mod->name() + ".sv");
    files.insert(vf);
    std::ostream& os = vf->openStream();
    Context ctxt(os, mod);

    os << header;
    os << "\n";
    os << "// RTL Wrapper for " << mod->wrapped()->name() << "\n";
    os << "module " << mod->name() << " (\n";

    const auto& pinDefs = mod->pinDefs();
    for (auto& pr: pinDefs) {
        Port* port = pr.first;
        string recvIn;
        string recvOut;
        if (port->asInput() == nullptr) {
            recvIn = "output wire";
            recvOut = "input wire";
        } else {
            recvIn = "input wire";
            recvOut = "output wire";
        }

        RTLTranslator* trans = pr.second;
        LIBus* li = dynamic_cast<LIBus*>(trans);
        if (li == nullptr)
            throw InvalidArgument("Sorry, the verilog synthesizer only knows "
                                  "how to use the LIBus RTL translator");
        auto pins = trans->pins();
        os << "    " << recvIn << " " << li->valid().name() << ",\n";
        os << "    " << recvOut << " " << li->bp().name() << ",\n";
        deque<llvm::Type*> types;
        if (numContainedTypes(port->type()) == 0) {
            types.push_back(port->type());
        } else {
            for (unsigned i=0; i<numContainedTypes(port->type()); i++) {
                types.push_back(nthType(port->type(), i));
            }
        }

        pins.erase(pins.begin());
        pins.erase(pins.begin());
        for (const auto& pin: pins) {
            auto type = types.front();
            types.pop_front();
            if (bitwidth(type) > 0) {
                os << boost::format(
                    "    %1% [%2%-1:0] %3%, // %4%\n")
                    % recvIn
                    % bitwidth(type)
                    % pin.name()
                    % typestr(type);
            }
        }
        os << "\n";
    }

    os << "    input wire clk,\n"
       << "    input wire resetn\n"
       << ");\n";

    for (auto& pr: pinDefs) {
        RTLTranslator* trans = pr.second;
        LIBus* li = dynamic_cast<LIBus*>(trans);
        if (li == nullptr)
            throw InvalidArgument("Sorry, the verilog synthesizer only knows "
                                  "how to use the LIBus RTL translator");
        const auto& pins = li->pins();
        Port* port = pr.first;
        if (port->asInput() != nullptr) {
            // This is an input port
            auto ip = port->asInput();
            ctxt << boost::format(
                "    wire %1%_valid = %2%;\n"
                "    wire %1%_bp;\n"
                "    assign %3% = %4% %1%_bp;\n")
                % ctxt.name(ip)
                % li->valid().name()
                % li->bp().name()
                % (li->bpStyle() == LIBus::BPStyle::Wait ? " " : "!") ;

            ctxt << boost::format(
                "    wire [%1%-1:0] %2% = {\n")
                % bitwidth(ip->type())
                % ctxt.name(ip);
            for (unsigned i=0; i<pins.size(); i++) {
                Pin pin = pins[pins.size() - i - 1];
                if (i == pins.size() - 1)
                    ctxt << "        " << pin.name() << "\n";
                else
                    ctxt << "        " << pin.name() << ",\n";
            }
            ctxt << "    };\n";
        } else {
            // This is an output port
            auto op = port->asOutput();
            ctxt << boost::format(
                "    wire %1%_valid;\n"
                "    assign %2% = %1%_valid;\n"
                "    wire %1%_bp = %4% %3%;\n")
                % ctxt.name(op)
                % li->valid().name()
                % li->bp().name()
                % (li->bpStyle() == LIBus::BPStyle::Wait ? " " : "!") ;

            ctxt << boost::format(
                "    wire [%1%-1:0] %2%;\n")
                % bitwidth(op->type())
                % ctxt.name(op);
            if (pins.size() == 1) {
                ctxt << "    assign " << pins[0].name() << " = "
                                      << ctxt.name(op) << ";\n";
            } else {
                for (unsigned i=2; i<pins.size(); i++) {
                    unsigned offset, width;
                    if (numContainedTypes(op->type()) > 0) {
                        offset = bitoffset(op->type(), i-2);
                        width = bitwidth(nthType(op->type(), i-2));
                    } else {
                        offset = 0;
                        width = bitwidth(op->type());
                    }
                    Pin pin = pins[i];
                    ctxt << boost::format(
                        "    assign %1% = %2%[%3%:%4%];\n")
                        % pin.name()
                        % ctxt.name(op)
                        % (offset + width - 1)
                        % offset;
                }
            }
        }
    }

    // Print the module instantiation
    const auto& possible_printers = _printers(mod->wrapped());
    assert(possible_printers.size() > 0);
    auto printer = possible_printers.front();
    printer->print(ctxt, mod->wrapped());
    os << "endmodule\n";

    vf->close();
    writeModule(dir, mod->wrapped(), files);
}

} // namespace llpm
