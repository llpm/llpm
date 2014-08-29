#include "synthesize.hpp"

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
    printf("Scheduling...\n");
    Schedule* s = mod->schedule();
    printf("Pipelining...\n");
    Pipeline* p = mod->pipeline();
}

} // namespace llpm
