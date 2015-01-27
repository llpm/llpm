#ifndef __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__
#define __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__

#include <util/files.hpp>
#include <wedges/wedge.hpp>
#include <backends/verilog/synthesize.hpp>

namespace llpm {
// Fwd defs
class IPXactBackend;

class AXIWedge : public Wedge {
    friend class AXIWrapper;

    enum class ChannelType {
        WriteAddress,
        WriteData,
        WriteResp,
        ReadAddress,
        ReadData
    };

    Design& _design;
    std::map<Port*, ChannelType> _types;
    static std::vector<std::string> getPinNames(ChannelType ct);
    std::vector<std::string> getPinNames(Port*) const; 
    void writeMetadata(Module* mod, IPXactBackend* ipxact);
public:
    AXIWedge(Design& design) :
        _design(design) 
    { }

    virtual ~AXIWedge() { }

    DEF_GET_NP(design);

    virtual Module* wrapModule(Module* mod);
    virtual void writeModule(FileSet& fileset, Module* mod);
};

} // namespace llpm

#endif // __LLPM_WEDGES_AXI_AXI_WEDGE_HPP__

