#include "axi4_lite.hpp"

#include <llpm/module.hpp>
#include <llpm/connection.hpp>
#include <util/llvm_type.hpp>
#include <util/misc.hpp>
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <libraries/core/std_library.hpp>
#include <libraries/synthesis/pipeline.hpp>

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
    _addrOffset(addr_offset),
    _currOffset(0)
{ 
    if (!(data_width == 32 || data_width == 64)) {
        throw InvalidArgument("AXI4-Lite requires data widths of 32 or 64 bits");
    }

    _writeInputs = {writeAddress(), writeData()};
    _readInputs = {readAddress()};
}

void AXI4LiteSlaveAdapter::connect(ConnectionDB* conns, InputPort* ip) {
    auto driver = new OutputPort(this, ip->type(), ip->name() + "_axi");
    _inputDrivers.push_back(driver);
    conns->connect(driver, ip);
    assignPort(driver);
}

void AXI4LiteSlaveAdapter::connect(ConnectionDB* conns, OutputPort* op) {
    auto sink = new InputPort(this, op->type(), op->name() + "_axi");
    _outputSinks.push_back(sink);
    _readInputs.push_back(sink);
    conns->connect(op, sink);
    assignPort(sink);
}

DependenceRule AXI4LiteSlaveAdapter::deps(const OutputPort* op) const {
    if (op == readData()) {
        return DependenceRule(DependenceRule::Custom, _readInputs);
    }
    if (op == writeResp()) {
        return DependenceRule(DependenceRule::AND_FireOne, _writeInputs);
    }
    return DependenceRule(DependenceRule::Custom, inputs());
}

void AXI4LiteSlaveAdapter::assignPort(Port* port) {
    // Round the existing offset up to the nearest 8-byte aligned
    // address
    unsigned dataWidthBytes = _dataWidth / 8;
    if (_currOffset % dataWidthBytes != 0)
        _currOffset += dataWidthBytes - (_currOffset % dataWidthBytes);

    unsigned start = _currOffset;
    unsigned width = bitwidth(port->type());
    unsigned byteWidth = (width + 7) / 8;
    if (byteWidth == 0)
        byteWidth = 1;
    _currOffset += byteWidth;
    unsigned end = _currOffset;
    _offsetRanges[port] = make_pair(start, end);
    if (auto op = port->asOutput()) {
        printf("Assigning %s to (%u, %u)\n",
               module()->design().namer().getName(op, module()).c_str(),
               start, end);
    } else if (auto ip = port->asInput()) {
        printf("Assigning %s to (%u, %u)\n",
               module()->design().namer().getName(ip, module()).c_str(),
               start, end);
    }
}

void AXI4LiteSlaveAdapter::refineWriteSide(ConnectionDB& conns) const {
    // Build write side
    auto& ctxt = writeAddress()->type()->getContext();
    auto addr = new Extract(writeAddress()->type(), {0});
    auto waReg = new PipelineRegister(this->writeAddress());
    conns.connect(waReg->dout(), addr->din());
    conns.remap(this->writeAddress(), {waReg->din()});
    auto glblOffset = new Constant(
        llvm::Constant::getIntegerValue(
            llvm::Type::getIntNTy(ctxt, _addrWidth),
            llvm::APInt(_addrWidth, _addrOffset)));
    auto offset = new IntSubtraction(addr->dout()->type(),
                                     glblOffset->dout()->type());
    conns.connect(addr->dout(), offset->din()->join(conns)->din(0));
    conns.connect(glblOffset->dout(), offset->din()->join(conns)->din(1));
    auto regSel = new ConstShift(offset->dout()->type(),
                                 (_dataWidth == 32) ? -2 : -3,
                                 ConstShift::LogicalTruncating);
    conns.connect(offset->dout(), regSel->din());
    auto data = new Extract(writeData()->type(), {0});
    auto wdReg = new PipelineRegister(this->writeData());
    conns.connect(wdReg->dout(), data->din());
    conns.remap(this->writeData(), {wdReg->din()});

    unsigned maxOffset = 0;
    for (auto driver: _inputDrivers) {
        unsigned endOffset = offsetRange(driver).second;
        maxOffset = std::max(endOffset, maxOffset);
    }
    const unsigned dataWidthBytes = _dataWidth / 8;
    const unsigned numVirtRegs =
        (maxOffset + dataWidthBytes - 1)
            / dataWidthBytes;

    // {virtual reg selector, data}
    auto regSelTrunc = new IntTruncate(
        regSel->dout()->type(),
        llvm::Type::getIntNTy(ctxt, idxwidth(numVirtRegs)));
    conns.connect(regSel->dout(), regSelTrunc->din());
    auto selAndData = Join::get(conns, {regSelTrunc->dout(), data->dout()});
    auto router = new Router(numVirtRegs, data->dout()->type());
    conns.connect(selAndData->dout(), router->din());
    for (auto driver: _inputDrivers) {
        auto width = bitwidth(driver->type());
        auto offsetR = offsetRange(driver);
        auto regBegin = offsetR.first / dataWidthBytes;
        auto regEnd = (offsetR.second + dataWidthBytes - 1) / dataWidthBytes;
        vector<OutputPort*> ports;
        for (unsigned i=regBegin; i<regEnd; i++) {
            ports.push_back(router->dout(i));
        }

        if (width % _dataWidth != 0) {
            auto trunc = new IntTruncate(
                _dataWidth - (width % _dataWidth),
                ports.back()->type());
            conns.connect(ports.back(), trunc->din());
            ports.pop_back();
            ports.push_back(trunc->dout());
        }

        auto outJoin = Join::get(conns, ports);
        if (width > 0) {
            auto outCast = new Cast(outJoin->dout()->type(),
                                    driver->type());
            conns.connect(outJoin->dout(), outCast->din());
            conns.remap(driver, outCast->dout());
        } else {
            auto outConst = Constant::getVoid(module()->design());
            auto outWait = new Wait(outConst->dout()->type());
            conns.connect(outConst->dout(), outWait->din());
            outWait->newControl(&conns, outJoin->dout());
            conns.remap(driver, outWait->dout());
        }
    }

    auto wrespConst = new Constant(
        llvm::Constant::getIntegerValue(
            llvm::Type::getIntNTy(ctxt, 2),
            llvm::APInt(2, 0)));
    auto wresp = Join::get(conns, {wrespConst->dout()});
    auto wRespWait = new Wait(wresp->dout()->type());
    conns.connect(wresp->dout(), wRespWait->din());
    wRespWait->newControl(&conns, selAndData->dout());
    conns.remap(writeResp(), wRespWait->dout());
}

void AXI4LiteSlaveAdapter::refineReadSide(ConnectionDB& conns) const {
    // Build read side
    auto& ctxt = readAddress()->type()->getContext();
    auto dataWidthInt = llvm::Type::getIntNTy(ctxt, _dataWidth);

    auto addr = new Extract(readAddress()->type(), {0});
    auto raReg = new PipelineRegister(this->readAddress());
    conns.connect(raReg->dout(), addr->din());
    conns.remap(this->readAddress(), {raReg->din()});
    auto glblOffset = new Constant(
        llvm::Constant::getIntegerValue(
            llvm::Type::getIntNTy(addr->din()->type()->getContext(),
                                  _addrWidth),
            llvm::APInt(_addrWidth, _addrOffset)));
    auto offset = new IntSubtraction(addr->dout()->type(),
                                     glblOffset->dout()->type());
    conns.connect(addr->dout(), offset->din()->join(conns)->din(0));
    conns.connect(glblOffset->dout(), offset->din()->join(conns)->din(1));
    auto regSel = new ConstShift(offset->dout()->type(),
                                 (_dataWidth == 32) ? -2 : -3,
                                 ConstShift::LogicalTruncating);
    conns.connect(offset->dout(), regSel->din());

    unsigned maxOffset = 0;
    for (auto sink: _outputSinks) {
        unsigned endOffset = offsetRange(sink).second;
        maxOffset = std::max(endOffset, maxOffset);
    }

    const unsigned dataWidthBytes = _dataWidth / 8;
    const unsigned numVirtRegs =
        (maxOffset + dataWidthBytes - 1)
            / dataWidthBytes;

    auto regSelTrunc = new IntTruncate(
        regSel->dout()->type(),
        llvm::Type::getIntNTy(ctxt, idxwidth(numVirtRegs)));
    conns.connect(regSel->dout(), regSelTrunc->din());
    auto dummyConst = new Constant(llvm::Type::getVoidTy(ctxt));
    auto selAndDummy = Join::get(conns, {regSelTrunc->dout(), dummyConst->dout()});
    auto router = new Router(numVirtRegs, llvm::Type::getVoidTy(ctxt));
    conns.connect(selAndDummy->dout(), router->din());

    vector<OutputPort*> selInputs;
    for (auto sink: _outputSinks) {
        const auto width = bitwidth(sink->type());
        auto offsetR = offsetRange(sink);
        auto regBegin = offsetR.first / dataWidthBytes;
        auto regEnd = (offsetR.second + dataWidthBytes - 1) / dataWidthBytes;

        vector<llvm::Type*> castTypeVec;
        for (unsigned i=regBegin; i<regEnd; i++) {
            unsigned lclwidth = 
                (i != (regEnd-1)) ?
                    _dataWidth :
                    (width % _dataWidth);
            if (lclwidth == 0)
                // Special case: width of channel is zero. Deal with
                // it later on, but for now specify a full-width type
                lclwidth = _dataWidth;
            castTypeVec.push_back(llvm::Type::getIntNTy(ctxt, lclwidth));
        }
        llvm::StructType* castType = llvm::StructType::get(ctxt, castTypeVec);
        OutputPort* castedIn;
        if (width > 0) {
            auto inCast = new Cast(sink->type(), castType);
            conns.remap(sink, {inCast->din()});
            castedIn = inCast->dout();
        } else {
            // Special case: width of channel is zero. Construct a
            // wait instead
            auto inConst = new Constant(
                                llvm::Constant::getIntegerValue(
                                    dataWidthInt,
                                    llvm::APInt(_dataWidth, 0)));
            auto inWait = new Wait(castType);
            conns.connect(inConst->dout(), inWait->din()->join(conns)->din(0));
            conns.remap(sink, inWait->newControl(sink->type()));
            castedIn = inWait->dout();
        }
        auto inSplit = castedIn->split(conns);

        for (unsigned i=regBegin; i<regEnd; i++) {
            OutputPort* op = inSplit->dout(i - regBegin);
            if (op->type()->getIntegerBitWidth() != _dataWidth) {
                int extAmt = _dataWidth - op->type()->getIntegerBitWidth();
                assert(extAmt > 0);
                auto ext = new IntExtend(extAmt, false, op->type());
                conns.connect(op, ext->din());
                op = ext->dout();
            }
            auto wait = new Wait(dataWidthInt);
            conns.connect(op, wait->din());
            wait->newControl(&conns, router->dout(i));  
            selInputs.push_back(wait->dout());
        }
    }
    auto outSel = new Select(selInputs.size(), dataWidthInt);
    for (unsigned i=0; i<selInputs.size(); i++) {
        conns.connect(selInputs[i], outSel->din(i));
    }

    auto rresp = new Constant(
        llvm::Constant::getIntegerValue(
            llvm::Type::getIntNTy(ctxt, 2),
            llvm::APInt(2, 0)));
    auto outJoin = Join::get(conns, {outSel->dout(), rresp->dout()});
    conns.remap(readData(), outJoin->dout());
}

bool AXI4LiteSlaveAdapter::refine(ConnectionDB& conns) const {
    refineWriteSide(conns);
    refineReadSide(conns);
    return true;
}

} // namespace llpm
