#include "synthesize.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>
#include <util/llvm_type.hpp>

#include <boost/format.hpp>

namespace llpm {

VerilogSynthesizer::VerilogSynthesizer(Design& d) :
    _design(d)
{ }

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
    auto blockName = namer.getName(b, ctxt);
    
    for (auto&& ip: b->inputs()) {
        auto inputName = namer.getName(ip, ctxt);
        auto width = bitwidth(ip->type());
        os << boost::format("    wire [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % inputName
                % typestr(ip->type());
    }
    for (auto&& op: b->outputs()) {
        auto outputName = namer.getName(op, ctxt);
        auto width = bitwidth(op->type());
        os << boost::format("    wire [%1%:0] %2%; //Type: %3%\n")
                % (width - 1)
                % outputName
                % typestr(op->type());
    }

    os << boost::format("    block %1%;\n") % blockName;
    os << "\n";
}

} // namespace llpm
