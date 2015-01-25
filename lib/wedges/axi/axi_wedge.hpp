#ifndef __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__
#define __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__

#include <util/files.hpp>
#include <wedges/wedge.hpp>
#include <backends/verilog/synthesize.hpp>

namespace llpm {

class AXIWedge : public Wedge {
    Design& _design;
public:
    AXIWedge(Design& design) :
        _design(design) 
    { }

    virtual ~AXIWedge() { }

    DEF_GET_NP(design);

    virtual void writeModule(FileSet& fileset, Module* mod);
};

} // namespace llpm

#endif // __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__

