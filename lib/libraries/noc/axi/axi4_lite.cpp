#include "axi4_lite.hpp"

#include <llpm/connection.hpp>

#include <llvm/IR/DerivedTypes.h>

using namespace std;

namespace llpm {

AXI4LiteSlaveAdapter::AXI4LiteSlaveAdapter(unsigned addr_width,
                 unsigned data_width,
                 uint64_t addr_offset,
                 llvm::LLVMContext& ctxt) :
    _writeAddress(
        this,
        llvm::StructType::get(llvm::Type::getIntNTy(ctxt, addr_width),
                              llvm::Type::getIntNTy(ctxt, 3),
                              NULL),
        "write_address"),
    _writeData(
        this, 
        llvm::StructType::get(llvm::Type::getIntNTy(ctxt, data_width),
                              llvm::Type::getIntNTy(ctxt, data_width/8),
                              NULL),
        "write_data"),
    _writeResp(
        this,
        llvm::StructType::get(llvm::Type::getIntNTy(ctxt, 2),
                              NULL),
        "write_resp"),
    _readAddress(
        this,
        llvm::StructType::get(llvm::Type::getIntNTy(ctxt, addr_width),
                              llvm::Type::getIntNTy(ctxt, 3),
                              NULL),
        "read_address"),
    _readData(
        this,
        llvm::StructType::get(llvm::Type::getIntNTy(ctxt, data_width),
                              llvm::Type::getIntNTy(ctxt, 2),
                              NULL),
        "read_data"),
    _addrWidth(addr_width),
    _dataWidth(data_width),
    _addrOffset(addr_offset)
{ 
    if (!(data_width == 32 || data_width == 64)) {
        throw InvalidArgument("AXI4-Lite requires data widths of 32 or 64 bits");
    }

    _writeInputs = {writeAddress(), writeData()};
    _readInputs = {readAddress()};
}

void AXI4LiteSlaveAdapter::connect(ConnectionDB* conns, InputPort* ip) {
    auto driver = new OutputPort(this, ip->type());
    _inputDrivers.push_back(driver);
    conns->connect(driver, ip);
}

void AXI4LiteSlaveAdapter::connect(ConnectionDB* conns, OutputPort* op) {
    auto sink = new InputPort(this, op->type());
    _outputSinks.push_back(sink);
    _readInputs.push_back(sink);
    conns->connect(op, sink);
}

DependenceRule AXI4LiteSlaveAdapter::depRule(const OutputPort* op) const {
    if (op == readData()) {
        return DependenceRule(DependenceRule::Custom, DependenceRule::Maybe);
    }
    if (op == writeResp()) {
        return DependenceRule(DependenceRule::AND, DependenceRule::Always);
    }
    return DependenceRule(DependenceRule::AND, DependenceRule::Maybe);
}

const std::vector<InputPort*>&
AXI4LiteSlaveAdapter::deps(const OutputPort* op) const {
    if (op == readData()) {
        return _readInputs;
    }
    return _writeInputs;
}

} // namespace llpm
