#ifndef __LLPM_BACKENDS_IPXACT_IPXACT_HPP__
#define __LLPM_BACKENDS_IPXACT_IPXACT_HPP__

#include <backends/backend.hpp>
#include <backends/verilog/synthesize.hpp>

namespace llpm {

class IPXactBackend : public Backend {
public:
    struct Port {
        std::string name;
        bool input;
        unsigned width;

        Port(std::string name, bool input, unsigned width=1) :
            name(name),
            input(input),
            width(width)
        { }
    };

    struct MemoryMap {
        std::string name;
        uint64_t baseAddress;
        uint64_t range;
        uint64_t width;

        MemoryMap(std::string name, uint64_t base,
                  uint64_t range, uint64_t width) :
            name(name), baseAddress(base), range(range), width(width)
        { }

        MemoryMap()
        { }
    };

    struct BusInterface {
        std::string name;
        std::string mmName;
        bool littleEndian;
        std::map<std::string, std::string> busType;
        std::map<std::string, std::string> abstractionType;
        std::map<std::string, std::string> portMaps;
        std::map<std::string, std::map<std::string, std::string>> parameters;

        BusInterface(std::string name, bool littleEndian = true) :
            name(name),
            littleEndian(littleEndian)
        { }
    };

private:
    VerilogSynthesizer _verilog;
    std::vector<Port> _ports;
    std::vector<BusInterface> _busInterfaces;
    std::vector<MemoryMap> _memoryMaps;
    std::set<std::string> _pkgFiles;

    void buildClkRst();
    void writeComponent(FileSet::File* xmlFile, Module* mod);

public:
    IPXactBackend(Design& design);

    virtual ~IPXactBackend() { }

    virtual bool blockIsPrimitive(Block* b) {
        return _verilog.blockIsPrimitive(b);
    }
    virtual Refinery::StopCondition* primitiveStops() {
        return _verilog.primitiveStops();
    }

    virtual bool synchronous() const {
        return _verilog.synchronous();
    }

    virtual void writeModule(FileSet& dir,
                             Module* mod,
                             std::set<FileSet::File*>& files);

    void push(Port p) {
        _ports.push_back(p);
    }
    void push(BusInterface bi) {
        _busInterfaces.push_back(bi);
    }
    void push(MemoryMap mm) {
        _memoryMaps.push_back(mm);
    }
};

} // namespace llpm

#endif // __LLPM_BACKENDS_IPXACT_IPXACT_HPP__
