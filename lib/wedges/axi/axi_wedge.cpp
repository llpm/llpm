#include "axi_wedge.hpp"

#include <libraries/noc/axi/axi4_lite.hpp>
#include <libraries/legacy/rtl_wrappers.hpp>

using namespace std;

namespace llpm {

class AXIWrapper : public ContainerModule {
public:
    AXIWrapper(AXIWedge& wedge, Module* mod);
    virtual ~AXIWrapper() { }
};

AXIWrapper::AXIWrapper(AXIWedge& wedge, Module* mod) :
    ContainerModule(mod->design(), mod->name() + "_axi")
{
    auto adapter = new AXI4LiteSlaveAdapter(32, 64, 0x80000000,
                                            mod->design().context());
    adapter->name(mod->name() + "_axi");
    for (auto ip: mod->inputs()) {
        adapter->connect(conns(), ip);
    }

    for (auto op: mod->outputs()) {
        adapter->connect(conns(), op);
    }

    wedge._types[addInputPort(adapter->writeAddress(), "writeAddress")] =
        AXIWedge::ChannelType::WriteAddress;
    wedge._types[addInputPort(adapter->writeData(), "writeData")] =
        AXIWedge::ChannelType::WriteData;
    wedge._types[addOutputPort(adapter->writeResp(), "writeResp")] =
        AXIWedge::ChannelType::WriteResp;
    
    wedge._types[addInputPort(adapter->readAddress(), "readAddress")] =
        AXIWedge::ChannelType::ReadAddress;
    wedge._types[addOutputPort(adapter->readData(), "readData")] =
        AXIWedge::ChannelType::ReadData;
}

Module* AXIWedge::wrapModule(Module* mod) {
    // Create a wrapper module which contains the adapter
    auto wrapper = new AXIWrapper(*this, mod);
    return wrapper;
}
void AXIWedge::writeModule(FileSet& dir, Module* mod) {
    auto wrapper = new WrapLLPMMModule(mod);
    for (auto ip: mod->inputs()) {
        vector<string> names = getPinNames(ip);
        wrapper->specifyTranslator(
            ip,
            new LIBus(ip, LIBus::BPStyle::Ready, names));
    }
    for (auto op: mod->outputs()) {
        vector<string> names = getPinNames(op);
        wrapper->specifyTranslator(
            op,
            new LIBus(op, LIBus::BPStyle::Ready, names));
    }

    set<FileSet::File*> files;
    _design.backend()->writeModule(dir, wrapper, files);
}

std::vector<std::string> AXIWedge::getPinNames(Port* port) const {
    auto f = _types.find(port);
    if (f == _types.end())
        throw InvalidArgument("Could not find type for port!");
    switch (f->second) {
    case ChannelType::WriteAddress:
        return {"awvalid", "awready", "awaddr", "awprot"};
    case ChannelType::WriteData:
        return {"wvalid", "wready", "wdata", "wstrb"};
    case ChannelType::WriteResp:
        return {"bvalid", "bready", "bresp"};
    case ChannelType::ReadAddress:
        return {"arvalid", "arready", "araddr", "arprot"};
    case ChannelType::ReadData:
        return {"rvalid", "rready", "rdata", "rresp"};
    }
    assert(false && "Unanticipated channel type!");
}

} // namespace llpm
