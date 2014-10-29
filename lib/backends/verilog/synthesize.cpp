#include "synthesize.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>
#include <util/llvm_type.hpp>

#include <llpm/comm_intr.hpp>
#include <llpm/std_library.hpp>

#include <llvm/IR/Constants.h>

#include <boost/format.hpp>

namespace llpm {

VerilogSynthesizer::VerilogSynthesizer(Design& d) :
    _design(d)
{
    addDefaultPrinters();
}

void VerilogSynthesizer::write(std::ostream& os) {
    auto& modules = _design.modules();
    for (auto& m: modules) {
        writeModule(os, m);
    }
}

static const std::string header = R"STRING(
/*****************
 *  This code autogenerated by LLPM.
 *  DO NOT MODIFY MANUALLY!
 *  Manual changes will be overwritten.
 ******/

// Macros used in autogenerated verilog
`define DFF(W, N, D, CE)\
        reg [W-1:0] N;\
        always@(posedge clk, negedge resetn)\
        begin\
            if (resetn)\
                N <= {W{1'b0}};\
            else if (CE)\
                N <= D;\
        end
`define LI_CONTROL(V_OUT, V_IN, BP_OUT, BP_IN, CE)\
        reg V_OUT``_reg;\
        wire V_OUT = V_OUT``_reg && ~BP_IN;\
        wire BP_OUT = V_OUT;\
        wire CE = V_IN && ~BP_OUT;\
        always@(posedge clk, negedge resetn)\
        begin\
            if (resetn)\
                V_OUT <= 1'b0;\
            else begin\
                if (!BP_IN)\
                    V_OUT <= 1'b0;\
                if (V_IN)\
                    V_OUT <= 1'b1;\
            end\
        end
)STRING";

void VerilogSynthesizer::writeModule(FileSet::File* f, Module* mod) {
    writeModule(f->openStream(), mod);
    f->close();
}

void VerilogSynthesizer::writeModule(std::ostream& os, Module* mod) {
    Context ctxt(os, mod);

    printf("Scheduling...\n");
    Schedule* s = mod->schedule();
    printf("Pipelining...\n");
    Pipeline* p = mod->pipeline();

    // Write out header
    ctxt << header;

    ctxt << "\n\n";
    ctxt << "// The \"" << mod->name() << "\" module of type " << typeid(*mod).name() << "\n";
    ctxt << "module " << mod->name() << " (\n";


    ctxt.namer().reserveName("clk", mod);
    ctxt.namer().reserveName("resetn", mod);

    for (auto&& ip: mod->inputs()) {
        auto inputName = ctxt.name(ip, true);
        auto width = bitwidth(ip->type());
        ctxt << boost::format("    input [%1%:0] %2%, //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
        ctxt << boost::format("    input %1%_valid,\n") % inputName;
        ctxt << boost::format("    output %1%_bp,\n") % inputName;
    }
    ctxt << "\n";
    for (auto&& op: mod->outputs()) {
        auto outputName = ctxt.name(op, true);
        auto width = bitwidth(op->type());
        ctxt << boost::format("    output [%1%:0] %2%, //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
        ctxt << boost::format("    output %1%_valid,\n") % outputName;
        ctxt << boost::format("    input %1%_bp,\n") % outputName;
    }

    ctxt << "\n"
         << "    input clk,\n"
         << "    input resetn\n"
         << ");\n\n";
    

    ConnectionDB* conns = mod->conns();
    if (conns == NULL) {
        throw InvalidArgument("Verilog synthesizer cannot operate on opaque modules!");
    }

    ctxt << "    // Input registers\n";
    for (auto&& ip: mod->inputs()) {
        if (conns->findSource(ip) == NULL) {
            // If necessary, create a dummy block for each input
            Identity* dummy = new Identity(ip->type());
            conns->connect(dummy->dout(), ip);
        }
        OutputPort* dummyOP = conns->findSource(ip);
        ctxt.updateMapping(dummyOP, ctxt.name(dummyOP) + "_reg");

        ctxt << boost::format("    `LI_CONTROL(%1%_reg_valid, %1%_valid,\n"
                              "                %1%_reg_bp, %1%_bp,\n"
                              "                %1%_ce);\n")
                    % ctxt.name(ip);
        ctxt << boost::format("    `DFF(%1%, %2%_reg, %2%, %2%_ce);\n")
                    % bitwidth(ip->type())
                    % ctxt.name(ip);
    }
    ctxt << "\n";

    for (StaticRegion* region: s->regions()) {
        ctxt.region = region;
        unsigned regCount = region->id();
        ctxt << "// --- Static region " << regCount << " --- \n\n";

        vector< StaticRegion::Layer > stages;
        region->schedule(p, stages);

        // Examine all connections concerned with this region
        map<Block*, set<OutputPort*>> users;
        map<OutputPort*, unsigned> numUsers;
        set<OutputPort*> outputs;
        set<OutputPort*> inputs;
        for (auto&& block: region->blocks()) {
            vector<Connection> blockConns;
            conns->find(block, blockConns);

            for (auto c: blockConns) {
                numUsers[c.source()] += 1;
                switch (region->classifyConnection(c)) {
                case StaticRegion::Input:
                    inputs.insert(c.source());
                    users[block].insert(c.source());
                    break;
                case StaticRegion::Internal:
                    users[block].insert(c.source());
                    break;
                case StaticRegion::Output:
                    outputs.insert(c.source());
                    break;
                }
            }
        }

        ctxt << "    // Input LI control\n";
        ctxt << "    wire sr" << regCount << "_input_valid = \n";
        bool first = true;
        for (auto&& ip: inputs) {
            if (first)
                first = false;
            else 
                ctxt << " &\n";
            ctxt << "        " << ctxt.name(ip) << "_valid";
        }
        if (first)
            ctxt << "        1'b1;\n";
        else
            ctxt << ";\n";

        ctxt << boost::format("    `LI_CONTROL(sr%1%_l0_valid, sr%1%_input_valid,\n"
                              "                sr%1%_bp_out, sr%1%_bp_l0, sr%1%_l0_ce);\n") % regCount;

        ctxt << "    // Input registers\n";
        for (auto&& op: inputs) {
            std::string name = str(boost::format("%1%_sr%2%_l0")
                                        % ctxt.name(op)
                                        % regCount);

            ctxt << "    wire " << ctxt.name(op) << "_sr" << regCount << "bp = sr" << regCount << "_bp_out;\n";
            if (!op->type()->isVoidTy())
                ctxt << boost::format("    `DFF(%1%, %2%, %3%_extern, sr%4%_l0_ce);\n")
                            % bitwidth(op->type())
                            % name
                            % ctxt.name(op)
                            % regCount;
            ctxt.updateMapping(op, name);
        }
        ctxt << "\n";

        for (unsigned i=0; i<stages.size(); i++) {
            ctxt.layerNum = i;
            ctxt << "    // --- Pipeline stage " << i << " ---\n\n";

            ctxt.layer = &stages[i];
            auto blocks = ctxt.layer->blocks();
            for (Block* b: blocks) {
                print(ctxt, b);
            }

            for (Block* b: blocks) {
                auto uses = users[b];
                for (auto u: uses) {
                    auto f = numUsers.find(u);
                    assert(f->second > 0);
                    assert(f->first != NULL);
                    f->second -= 1;
                    if (f->second == 0) {
                        ctxt.removeMapping(f->first);
                        numUsers.erase(f);
                    }
                }
            }

            ctxt << boost::format("    `LI_CONTROL(sr%1%_l%2%_valid, sr%1%_l%3%_valid,\n"
                                  "                sr%1%_bp_l%3%, sr%1%_bp_l%2%, sr%1%_l%2%_ce);\n")
                            % regCount
                            % (i+1)
                            % i;
            for (auto p: numUsers) {
                // Iterate through all of the values still in use
                // and register them for the next layer
                OutputPort* op = p.first;
                if (!op->type()->isVoidTy())
                    ctxt << boost::format("    `DFF(%1%, %2%_sr%3%_l%4%, %2%, sr%3%_l%4%_ce);\n")
                                % bitwidth(op->type())
                                % ctxt.name(op)
                                % regCount
                                % (i+1);
                ctxt.updateMapping(op, str(boost::format("%1%_sr%2%_l%3%")
                                                    % ctxt.name(op)
                                                    % regCount
                                                    % (i+1)));
            }
            ctxt << "\n";
        }

        ctxt << "    // Output assignments\n";
        for (auto op: outputs) {
            if (!op->type()->isVoidTy())
                ctxt << boost::format("    wire [%3%:0] %1%_extern = %2%;\n")
                                % ctxt.name(op)
                                % ctxt.findMapping(op)
                                % (bitwidth(op->type()) - 1);
            ctxt << boost::format("    wire %1%_valid = sr%2%_l%3%_valid;\n")
                            % ctxt.name(op)
                            % regCount
                            % stages.size();
        }

        ctxt << "    // Incoming backpressure\n";
        ctxt << boost::format("    wire sr%1%_bp_l%2% = \n")
                    % regCount
                    % stages.size();

        first = true;
        for (auto op: outputs) {
            vector<InputPort*> sinks;
            conns->findSinks(op, sinks);
            for (auto op: sinks) {
                auto sinkBlock = op->owner();
                auto sinkSR = s->findRegion(sinkBlock);
                if (first)
                    first = false;
                else
                    ctxt << " |\n";
                ctxt << boost::format("        sr%1%_bp_out")
                                % sinkSR->id();
            }
        }
        if (first)
            ctxt << "        1'b0;\n";
        else
            ctxt << ";\n";

        ctxt << "\n\n";
    }

    ctxt << "endmodule\n";
}

void VerilogSynthesizer::print(Context& ctxt, Block* b)
{
    ctxt << "    // Block \"" << ctxt.primBlockName(b) << "\" type " << typeid(*b).name() << "\n";

    for (auto&& ip: b->inputs()) {
        if (ip->type()->isVoidTy())
            continue;
        auto inputName = ctxt.name(ip);
        auto width = bitwidth(ip->type());
        ctxt << boost::format("    reg [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
    }
    for (auto&& op: b->outputs()) {
        if (op->type()->isVoidTy())
            continue;
        auto outputName = ctxt.name(op);
        auto width = bitwidth(op->type());
        ctxt << boost::format("    reg [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
    }

    const vector<Printer*>& possible_printers = _printers(b);
    if (possible_printers.size() == 0) {
        auto blockName = ctxt.name(b);
        throw ImplementationError(
            str(boost::format(" Cannot translate block %1% of type %2% into verilog.") 
                            % blockName
                            % typeid(*b).name()));
    } else {
        possible_printers.front()->print(ctxt, b);
    }

    ConnectionDB* conns = ctxt.module()->conns();
    for (auto&& ip: b->inputs()) {
        if (ip->type()->isVoidTy())
            continue;
        OutputPort* source = conns->findSource(ip);
        if (source == NULL) {
            printf("Warning: unconnected input: %s!\n", ctxt.name(ip).c_str());
        } else {
            auto sourceBlock = source->owner();
            if (ctxt.layer->contains(sourceBlock)) {
                // Source is in the same layer so just connect it!
                ctxt << "    assign " << ctxt.name(ip) << " = " << ctxt.name(source) << ";\n";
            } else {
                ctxt << "    assign " << ctxt.name(ip) << " = " << ctxt.findMapping(source) << ";\n";
            }
        }
    }
    ctxt << "\n";
}

void print_function(VerilogSynthesizer::Context& ctxt, Block* c,
                    const char* op, bool signedWrap) {
    Function* b = dynamic_cast<Function*>(c);
    ctxt << "    assign " << ctxt.name(b->dout()) << " = ";
    auto dinType = b->din()->type();
    assert(dinType->isStructTy());
    auto dinName = ctxt.name(b->din());
    bool first = true;
    for (unsigned i = 0; i < dinType->getNumContainedTypes(); i++) {
        unsigned offset = bitoffset(dinType, i);
        unsigned width = bitwidth(dinType->getContainedType(i));
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
        string msb = str(boost::format("%1%[%2]")
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
        Function* f = dynamic_cast<Function*>(c);
        if (!f->din()->type()->isVoidTy())
            ctxt << "    assign " << ctxt.name(f->dout()) << " = "
                 << ctxt.name(f->din()) << ";\n";
    }
};

class ConstantPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Constant*>(b) != NULL;
    }

    static std::string toString(llvm::Constant* c) {
        switch (c->getType()->getTypeID()) {
            case llvm::Type::IntegerTyID:
                return str(boost::format("%1%'d%2%") 
                            % bitwidth(c->getType())
                            % llvm::dyn_cast<llvm::ConstantInt>(c)->getValue().toString(10, true));
        default:
            throw InvalidArgument("Constant type not yet supported");
        }
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Constant* f = dynamic_cast<Constant*>(c);
        ctxt << "    assign " << ctxt.name(f->dout()) << " = "
             << toString(f->value()) << ";";
    }
};

class JoinPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Join*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Join* j = dynamic_cast<Join*>(c);
        ctxt << "    assign " << ctxt.name(j->dout()) << " = { ";
        bool first = true;
        for (size_t i=0; i < j->din_size(); i++) {
            if (j->din(i)->type()->isVoidTy())
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
            if (s->dout(i)->type()->isVoidTy())
                continue;
            unsigned offset = bitoffset(s->din()->type(), i);
            unsigned width = bitwidth(s->din()->type()->getContainedType(i));
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
            type = type->getContainedType(i);
        }

        unsigned width = bitwidth(e->dout()->type());

        ctxt << "    assign " << ctxt.name(e->dout()) << " = "
             << boost::format("%1%[%2%:%3%];\n") 
                    % dinName
                    % (offset+width-1)
                    % offset;
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
        unsigned nWidth = bitwidth(dinType->getContainedType(0));
        unsigned nOffset = bitoffset(dinType, 0);

        ctxt << "    always\n"
             << "    begin\n"
             << boost::format("        case (%1%[%2%:%3%])\n") 
                    % dinName
                    % (nWidth + nOffset - 1)
                    % nOffset;
        for (size_t i=1; i<dinType->getNumContainedTypes(); i++) {
            auto offset = bitoffset(dinType, i);
            ctxt << boost::format("            %1% : %2% = %3%[%4%:%5%];\n")
                        % (i - 1)
                        % doutName
                        % dinName
                        % (offset + bitwidth(dinType->getContainedType(i)) - 1)
                        % offset;
        }
        ctxt << "        endcase\n"
             << "    end\n";
    }
};

class RouterPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Router*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Router* r = dynamic_cast<Router*>(c);
        auto dinName = ctxt.name(r->din());
        auto dinType = r->din()->type()->getContainedType(1);
        auto dinOffset = bitoffset(r->din()->type(), 1);
        auto dinWidth = bitwidth(dinType);

        auto selType = r->din()->type()->getContainedType(0);
        auto selWidth = bitwidth(selType);
        auto selOffset = bitoffset(r->din()->type(), 0);

        for (unsigned i=0; i<r->dout_size(); i++) {
            auto op = r->dout(i);
            ctxt << "    assign " << ctxt.name(op) << " = "
                 << boost::format("%1%[%2%:%3%] == %4% ? %1%[%5%:%6%] : {%7%{1'bx}};\n")
                    % dinName
                    % (selWidth + selOffset - 1)
                    % selOffset
                    % i
                    % (dinWidth + dinOffset - 1)
                    % dinOffset
                    % dinWidth;
        }
    }
};

class SelectPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Select*>(b) != NULL;
    }

    void print(VerilogSynthesizer::Context& ctxt, Block* c) const {
        Select* s = dynamic_cast<Select*>(c);
        

    }
};

void VerilogSynthesizer::addDefaultPrinters() {
    _printers.appendEntry(new BinaryOpPrinter<IntAddition>("+"));
    _printers.appendEntry(new BinaryOpPrinter<IntSubtraction>("-"));
    _printers.appendEntry(new BinaryOpPrinter<IntMultiply>("*"));
    _printers.appendEntry(new BinaryOpPrinter<IntDivide>("/"));
    _printers.appendEntry(new BinaryOpPrinter<IntRemainder>("%"));

    _printers.appendEntry(new CompareOpPrinter());
    _printers.appendEntry(new BitwiseOpPrinter());

    _printers.appendEntry(new IntTruncatePrinter());
    _printers.appendEntry(new IntExtendPrinter());

    _printers.appendEntry(new IdentityOpPrinter<Identity>());
    _printers.appendEntry(new IdentityOpPrinter<Cast>());

    _printers.appendEntry(new ConstantPrinter());

    _printers.appendEntry(new JoinPrinter());
    _printers.appendEntry(new SelectPrinter());
    _printers.appendEntry(new SplitPrinter());
    _printers.appendEntry(new ExtractPrinter());

    _printers.appendEntry(new MultiplexerPrinter());
    _printers.appendEntry(new RouterPrinter());
}

} // namespace llpm
