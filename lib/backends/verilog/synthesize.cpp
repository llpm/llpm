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

void VerilogSynthesizer::writeModule(std::ostream& os, Module* mod) {
    Context ctxt(os, mod);

    printf("Scheduling...\n");
    Schedule* s = mod->schedule();
    printf("Pipelining...\n");
    Pipeline* p = mod->pipeline();

    ctxt << "module " << mod->name() << " (\n";


    ctxt.namer().reserveName("clk", mod);
    ctxt.namer().reserveName("resetn", mod);

    for (auto&& ip: mod->inputs()) {
        auto inputName = ctxt.name(ip);
        auto width = bitwidth(ip->type());
        ctxt << boost::format("    input [%1%:0] %2%, //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
    }
    for (auto&& op: mod->outputs()) {
        auto outputName = ctxt.name(op);
        auto width = bitwidth(op->type());
        ctxt << boost::format("    output [%1%:0] %2%, //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
    }

    ctxt << "\n"
        << "    input clk,\n"
        << "    input resetn\n"
        << ");\n\n";


    unsigned regCount = 0;
    for (StaticRegion* region: s->regions()) {
        regCount++;
        ctxt << "// --- Static region " << regCount << " --- \n\n";

        vector< StaticRegion::Layer > stages;
        region->schedule(p, stages);

        for (unsigned i=0; i<stages.size(); i++) {
            ctxt << "    // --- Pipeline stage " << i << " ---\n\n";

            StaticRegion::Layer& layer = stages[i];
            auto blocks = layer.blocks();
            for (Block* b: blocks) {
                print(ctxt, b);
            }
        }
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
