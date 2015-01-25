#ifndef __LLPM_LIBRARIES_NOC_AXI_AXI4_LITE_HPP__
#define __LLPM_LIBRARIES_NOC_AXI_AXI4_LITE_HPP__

#include <llpm/block.hpp>

namespace llpm {

/**
 * This adapter converts misc input/output channels into a memory
 * mapped AXI4Lite interface. For each input channel, it creates a
 * sufficient memory space to accept a single input token and fires
 * the token when all registers have been written to. For each output
 * channel, it creates a sufficient memory space which the output
 * channel "writes" into and each memory address can be read once to
 * read the corresponding bits of the token.
 *
 * Clients must write and read entire tokens or else behavior is
 * undefined. This block may -- in the future -- enforce this rule
 * with proper response codes.
 */
class AXI4LiteSlaveAdapter : public Block {
    /// Write address channel {addr, prot}
    InputPort _writeAddress;
    /// Write data channel {data, strobe}
    InputPort _writeData;
    /// Write response channel {resp}
    OutputPort _writeResp;

    std::vector<InputPort*> _writeInputs;

    /// Read address channel {addr, prot}
    InputPort _readAddress;
    /// Read data channel {data, resp}
    OutputPort _readData;

    std::vector<InputPort*> _readInputs;

    std::vector<OutputPort*> _inputDrivers;
    std::vector<InputPort*>  _outputSinks;

    unsigned _addrWidth;
    unsigned _dataWidth;
    uint64_t _addrOffset;

public:
    AXI4LiteSlaveAdapter(unsigned addr_width,
                         unsigned data_width,
                         uint64_t addr_offset,
                         llvm::LLVMContext&);

    virtual ~AXI4LiteSlaveAdapter() { }

    void connect(ConnectionDB*, InputPort*);
    void connect(ConnectionDB*, OutputPort*);


    DEF_GET(writeAddress);
    DEF_GET(writeData);
    DEF_GET(writeResp);

    DEF_GET(readAddress);
    DEF_GET(readData);

    DEF_GET_NP(addrWidth);
    DEF_GET_NP(dataWidth);
    DEF_GET_NP(addrOffset);

    virtual bool outputsTied() const {
        return false;
    }
    virtual bool outputsSeparate() const {
        return true;
    }
    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule depRule(const OutputPort*) const;
    virtual const std::vector<InputPort*>& deps(const OutputPort*) const;
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_NOC_AXI_AXI4_LITE_HPP__
