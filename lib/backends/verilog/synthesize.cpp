#include "synthesize.hpp"

#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>

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

    for (StaticRegion* region: s->regions()) {
        vector< StaticRegion::Layer > region_sched;
        region->schedule(p, region_sched);

        printf("-- region sched --\n");
        for (unsigned i=0; i<region_sched.size(); i++) {
            printf("   %u: %lu\n", i, region_sched[i].blocks().size());
        }
        printf("\n");
    }
}

} // namespace llpm
