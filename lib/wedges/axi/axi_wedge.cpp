#include "axi_wedge.hpp"

#include <libraries/noc/axi/axi4_lite.hpp>

using namespace std;

namespace llpm {

class AXIWrapper : public ContainerModule {
public:
    AXIWrapper(Module* mod);
    virtual ~AXIWrapper() { }
};

AXIWrapper::AXIWrapper(Module* mod) :
    ContainerModule(mod->design(), mod->name() + "_axiwrapper")
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

    addInputPort(adapter->writeAddress(), "writeAddres");
    addInputPort(adapter->writeData(), "writeData");
    addOutputPort(adapter->writeResp(), "writeResp");
    
    addInputPort(adapter->readAddress(), "readAddress");
    addOutputPort(adapter->readData(), "readData");
}

Module* AXIWedge::wrapModule(Module* mod) {
    // Create a wrapper module which contains the adapter
    auto wrapper = new AXIWrapper(mod);
    return wrapper;
}
void AXIWedge::writeModule(FileSet& dir, Module* mod) {
    set<FileSet::File*> files;
    _design.backend()->writeModule(dir, mod, files);
}

} // namespace llpm
