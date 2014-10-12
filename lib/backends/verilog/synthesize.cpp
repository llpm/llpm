#include "synthesize.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>
#include <util/llvm_type.hpp>

#include <llpm/comm_intr.hpp>
#include <llpm/std_library.hpp>

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
    auto& namer = mod->design().namer();

    printf("Scheduling...\n");
    Schedule* s = mod->schedule();
    printf("Pipelining...\n");
    Pipeline* p = mod->pipeline();

    os << "module " << mod->name() << " (\n";

    for (auto&& ip: mod->inputs()) {
        auto inputName = namer.getName(ip, mod);
        auto width = bitwidth(ip->type());
        os << boost::format("    input [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
    }
    for (auto&& op: mod->outputs()) {
        auto outputName = namer.getName(op, mod);
        auto width = bitwidth(op->type());
        os << boost::format("    output [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
    }

    os << ");\n";


    unsigned regCount = 0;
    for (StaticRegion* region: s->regions()) {
        regCount++;

        vector< StaticRegion::Layer > region_sched;
        region->schedule(p, region_sched);

        os << "    // Static region " << regCount << "\n";
        for (unsigned i=0; i<region_sched.size(); i++) {
            StaticRegion::Layer& layer = region_sched[i];
            auto blocks = layer.blocks();
            for (Block* b: blocks) {
                print(os, b, mod);
            }
        }
        os << "\n\n";
    }

    os << "endmodule\n";
}

void VerilogSynthesizer::print(std::ostream& os, Block* b, Module* ctxt)
{
    auto& namer = ctxt->design().namer();
    
    for (auto&& ip: b->inputs()) {
        if (ip->type()->isVoidTy())
            continue;
        auto inputName = namer.getName(ip, ctxt);
        auto width = bitwidth(ip->type());
        os << boost::format("    wire [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
    }
    for (auto&& op: b->outputs()) {
        if (op->type()->isVoidTy())
            continue;
        auto outputName = namer.getName(op, ctxt);
        auto width = bitwidth(op->type());
        os << boost::format("    wire [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
    }

    const vector<Printer*>& possible_printers = _printers(b);
    if (possible_printers.size() == 0) {
        auto blockName = namer.getName(b, ctxt);
        os << boost::format("    block %1%; // ERROR: CANNOT FORMAT BLOCK TYPE %2%\n") 
                            % blockName
                            % typeid(*b).name();
    } else {
        possible_printers.front()->print(os, namer, b, ctxt);
    }
    os << "\n";
}

void print_function(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt, const char* op) {
    Function* b = dynamic_cast<Function*>(c);
    os << "    assign " << namer.getName(b->dout(), ctxt) << " = ";
    auto dinType = b->din()->type();
    assert(dinType->isStructTy());
    auto dinName = namer.getName(b->din(), ctxt);
    bool first = true;
    for (unsigned i = 0; i < dinType->getNumContainedTypes(); i++) {
        unsigned offset = bitoffset(dinType, i);
        unsigned width = bitwidth(dinType->getContainedType(i));
        if (width == 0)
            continue;
        if (first)
            first = false;
        else
            os << " " << op << " ";
        os << boost::format("%1%[%2%:%3%]") 
                        % dinName
                        % (offset+width-1)
                        % offset;
    }
    os << "\n";
}

template<typename BlockType>
class BinaryOpPrinter : public VerilogSynthesizer::Printer {
    const char* _op;
public:
    BinaryOpPrinter(const char* op) : _op(op) { }

    bool handles(Block* b) const {
        return dynamic_cast<BlockType*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        print_function(os, namer, c, ctxt, _op);
    }
};

class CompareOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntCompare*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
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
        print_function(os, namer, c, ctxt, op);
    }
};

class BitwiseOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Bitwise*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
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
        print_function(os, namer, c, ctxt, op);
    }
};

class IntTruncatePrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntTruncate*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        Function* f = dynamic_cast<Function*>(c);
        os << "    assign " << namer.getName(f->dout(), ctxt) << " = "
           << boost::format("%1%[%2%:0];\n")
                            % namer.getName(f->din(), ctxt)
                            % (bitwidth(f->dout()->type()) - 1);
    }
};

class IntExtendPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<IntExtend*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        IntExtend* f = dynamic_cast<IntExtend*>(c);
        unsigned ib = bitwidth(f->din()->type());
        unsigned ob = bitwidth(f->dout()->type());
        assert(ob > ib);
        string msb = str(boost::format("%1%[%2]")
                            % namer.getName(f->din(), ctxt)
                            % (ib - 1));
        os << "    assign " << namer.getName(f->dout(), ctxt) << " = { "
           << boost::format("{%1%{%2%}}")
                % (ob - ib)
                % ( f->signExtend() ? msb : "1'b0")
           << ", " << namer.getName(f->din(), ctxt) << "};\n";
    }
};

template<typename Op>
class IdentityOpPrinter : public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Op*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        Function* f = dynamic_cast<Function*>(c);
        os << "    assign " << namer.getName(f->dout(), ctxt) << " = "
            << namer.getName(f->din(), ctxt) << ";\n";
    }
};

class JoinPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Join*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        Join* j = dynamic_cast<Join*>(c);
        os << "    assign " << namer.getName(j->dout(), ctxt) << " = { ";
        bool first = true;
        for (size_t i=0; i < j->din_size(); i++) {
            if (j->din(i)->type()->isVoidTy())
                continue;
            if (first)
                first = false;
            else
                os << ", ";
            os << namer.getName(j->din(i), ctxt);
        }
        os << " };\n";
    }
};

class SplitPrinter: public VerilogSynthesizer::Printer {
public:
    bool handles(Block* b) const {
        return dynamic_cast<Split*>(b) != NULL;
    }

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        Split* s = dynamic_cast<Split*>(c);
        auto dinName = namer.getName(s->din(), ctxt);
        for (unsigned i=0; i < s->dout_size(); i++) {
            if (s->dout(i)->type()->isVoidTy())
                continue;
            unsigned offset = bitoffset(s->din()->type(), i);
            unsigned width = bitwidth(s->din()->type()->getContainedType(i));
            os << "    assign " << namer.getName(s->dout(i), ctxt) << " = "
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

    void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const {
        Extract* e = dynamic_cast<Extract*>(c);
        auto dinName = namer.getName(e->din(), ctxt);
        unsigned offset = 0; 
        llvm::Type* type = e->din()->type();
        for (unsigned i: e->path()) {
            offset += bitoffset(type, i);
            type = type->getContainedType(i);
        }

        unsigned width = bitwidth(e->dout()->type());

        os << "    assign " << namer.getName(e->din(), ctxt) << " = "
           << boost::format("%1%[%2%:%3%];\n") 
                    % dinName
                    % (offset+width-1)
                    % offset;
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

    _printers.appendEntry(new JoinPrinter());
    _printers.appendEntry(new SplitPrinter());
    _printers.appendEntry(new ExtractPrinter());
}

} // namespace llpm
