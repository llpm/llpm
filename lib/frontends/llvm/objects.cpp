#include "objects.hpp"

#include <frontends/llvm/instruction.hpp>
#include <util/llvm_type.hpp>

#include <boost/format.hpp>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
#include <llvm/Support/Casting.h>

using namespace std;

namespace llpm {

#if 0
static bool isLocal(llvm::BasicBlock* bb, llvm::Value* v) {
    llvm::Instruction* operand = llvm::dyn_cast<llvm::Instruction>(v);
    return (operand != NULL && operand->getParent() == bb);
}
#endif

static bool isLocalOrConst(llvm::BasicBlock* bb, llvm::Value* v) {
    llvm::Instruction* ins  = llvm::dyn_cast<llvm::Instruction>(v);
    llvm::Constant*    cnst = llvm::dyn_cast<llvm::Constant>(v);
    return (ins  != NULL && ins->getParent() == bb) ||
           (cnst != NULL);
}

static bool isLocalArg(llvm::BasicBlock* bb, llvm::Value* v) {
    bool isEntry = &bb->getParent()->getEntryBlock() == bb;
    llvm::Argument*    arg     = llvm::dyn_cast<llvm::Argument>(v);
    return arg && isEntry;
}

static bool isLocalInstruction(llvm::BasicBlock* bb, llvm::Value* v) {
    llvm::Instruction* ins = llvm::dyn_cast<llvm::Instruction>(v);
    return ins && ins->getParent() == bb;
}

static void checkValue(llvm::Value* operand) {
    if (llvm::dyn_cast<llvm::Argument>(operand) ||
        llvm::dyn_cast<llvm::Instruction>(operand) ||
        llvm::dyn_cast<llvm::Constant>(operand))
        return;

    operand->dump();
    throw InvalidArgument("I don't know how to deal with value type!");
}

llvm::Type* LLVMFunction::sanitizeArgumentType(llvm::Argument* arg) {
    if (arg->getType()->isPointerTy() && arg->hasByValAttr()) {
        return arg->getType()->getPointerElementType();
    }
    return arg->getType();
}

llvm::Type* LLVMFunction::sanitizeType(llvm::Value* val) {
    if (llvm::Argument* arg = llvm::dyn_cast_or_null<llvm::Argument>(val)) {
        return sanitizeArgumentType(arg);
    }
    return val->getType();
}

std::string getBasicBlockName(llvm::BasicBlock* bb) {
    if (bb->hasName()) {
        return bb->getName();
    } else {
        auto& bbList = bb->getParent()->getBasicBlockList();
        size_t ctr = 0;
        for (auto& bi: bbList) {
            ctr += 1;
            if (&bi == bb)
                break;
        }
        return str(boost::format("bb%1%") % ctr);
    }
}

LLVMEntry::LLVMEntry(llvm::Function* func, const std::vector<llvm::Value*>& map) :
    StructTwiddler(FunctionType(func), ValueMap(func, map)) {
    this->name(func->getName().str() + "_entry");
}

LLVMExit::LLVMExit(llvm::Function* func, unsigned N) :
    Select(N, llvm::StructType::get(func->getContext(),
                                    vector<llvm::Type*>(1, func->getReturnType()) )) {
    // this->dout()->type()->dump(); printf("\n");
    this->name(func->getName().str() + "_exit");
}

LLVMBasicBlock::LLVMBasicBlock(LLVMFunction* f, llvm::BasicBlock* bb):
    _function(f),
    _basicBlock(bb),
    _returns(false),
    _numInputs(0),
    _numOutputs(0)
{
    this->name(getBasicBlockName(bb));

    llvm::TerminatorInst* ti = _basicBlock->getTerminator();
    if (ti == NULL) {
        throw InvalidArgument("A basic block does not appear to have a terminator!");
    }

    addOutput(ti);

    if (ti->getOpcode() == llvm::Instruction::Ret) {
        _returns = true;
    } else {
        for (unsigned i=0; i<ti->getNumSuccessors(); i++) {
            llvm::BasicBlock* successor = ti->getSuccessor(i);
            _successorMap[successor] = i;
        }
    }
}

LLVMPureBasicBlock::LLVMPureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb) :
        Function(NULL, NULL),
        LLVMBasicBlock(func, bb)
{ }

LLVMImpureBasicBlock::LLVMImpureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb) :
    LLVMBasicBlock(func, bb),
    _din(this, NULL, "din"),
    _dout(this, NULL, "dout")
{ }

DependenceRule LLVMImpureBasicBlock::depRule(
    const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    if (op == output())
        return DependenceRule(DependenceRule::AND, DependenceRule::Always);
    else
        return DependenceRule(DependenceRule::Custom, DependenceRule::Maybe);
}

const std::vector<InputPort*>& LLVMImpureBasicBlock::deps(
    const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    return inputs();
}



void LLVMBasicBlock::addInput(llvm::Value* v) {
    if (_inputMap.find(v) != _inputMap.end())
        return;
    const unsigned inputNum = _numInputs++;
    _inputMap[v].insert(inputNum);
    _nonPhiInputMap[v] = inputNum;
    unsigned pred_count = 0;
    for(auto iter = llvm::pred_begin(_basicBlock);
        iter != llvm::pred_end(_basicBlock); iter++) {

        auto pred = *iter;
        auto predBlock = _function->blockMap(pred);
        assert(predBlock);
        predBlock->requestOutput(v);
        _valueSources[v].insert(pred);
        pred_count += 1;
    }

    if (isLocalArg(_basicBlock, v)) {
        _valueSources[v].insert(NULL);
    } else if (pred_count == 0) {
        _basicBlock->dump();
        v->dump();
        assert(pred_count > 0);
    }
}

void LLVMBasicBlock::buildRequests() {
    std::set<llvm::BasicBlock*> predecessors(llvm::pred_begin(_basicBlock), llvm::pred_end(_basicBlock));

    std::vector<llvm::Type*> inputTypes;

    for(llvm::Instruction& ins: _basicBlock->getInstList()) {
        if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
            // PHI instructions are special
            inputTypes.push_back(LLVMInstruction::GetOutput(phi));
            _phiInputMap[phi] = _numInputs;
            assert(phi->getNumIncomingValues() > 0);
            for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                llvm::Value* v = phi->getIncomingValue(i);
                llvm::BasicBlock* pred = phi->getIncomingBlock(i);
                _valueSources[v].insert(pred);
                _inputMap[v].insert(_numInputs);

                auto predBlock = _function->blockMap(pred);
                assert(predBlock);
                predBlock->requestOutput(v);
            }
            _numInputs += 1;
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                if (LLVMInstruction::HWIgnoresOperand(&ins, i))
                    continue;

                llvm::Value* operand = ins.getOperand(i);
                assert(operand != NULL);
                checkValue(operand);

                if (!isLocalOrConst(_basicBlock, operand)) {
                    addInput(operand);
                }
            }

            if (ins.getNumUses() == 0 &&
                ins.getType()->isVoidTy()) {
                // If ins has no uses because it's a void, have it output the
                // void to make sure it influences control.
                requestOutput(&ins);
            }
        }
    }
}

void LLVMBasicBlock::requestOutput(llvm::Value* v) {
    if (_outputMap.find(v) != _outputMap.end())
        return;

    addOutput(v);
    if (isLocalInstruction(_basicBlock, v))
        return;

    if (llvm::Constant* cnst = llvm::dyn_cast<llvm::Constant>(v)) {
        _constants.insert(cnst);
    } else {
        _passthroughs.insert(v);
        addInput(v);
        if (!isLocalArg(_basicBlock, v)) {
            std::set<llvm::BasicBlock*> predecessors(llvm::pred_begin(_basicBlock), llvm::pred_end(_basicBlock));
            for(auto pred: predecessors) {
                auto predBlock = _function->blockMap(pred);
                predBlock->requestOutput(v);
            }
        }
    }
}

llvm::Type* LLVMBasicBlock::GetHWType(llvm::Value* v) {
    if (llvm::Instruction* i = llvm::dyn_cast<llvm::Instruction>(v)) {
        return LLVMInstruction::GetOutput(i);
    }
    return LLVMFunction::sanitizeType(v);
}

void LLVMBasicBlock::buildIO() {
    
    /**
     * Build Input Array
     */
    std::vector<llvm::Type*> inputs(_numInputs);

    for (auto& pr: _inputMap) {
        auto value = pr.first;
        for (auto idx: pr.second) {
            if (inputs[idx] != NULL) {
                assert(inputs[idx] == GetHWType(value));
            } else {
                inputs[idx] = GetHWType(value);
            }
        }
    }

    for (unsigned i=0; i<inputs.size(); i++) {
        assert(inputs[i] != NULL);
    }

    auto input = llvm::StructType::get(_basicBlock->getContext(), inputs);


    /**
     * Build output array
     */
    std::vector<llvm::Type*> outputs(_numOutputs);

    for (auto& pr: _outputMap) {
        auto value = pr.first;
        auto idx = pr.second;

        if (outputs[idx] != NULL) {
            assert(outputs[idx] == GetHWType(value));
        } else {
            outputs[idx] = GetHWType(value);
        }
    }

    for (auto& ot: outputs) {
        assert(ot != NULL);
    }

    auto output = llvm::StructType::get(_basicBlock->getContext(), outputs);

    // Reset I/O types
    this->resetTypes(input, output);
}

void LLVMImpureBasicBlock::buildIO() {
    LLVMBasicBlock::buildIO();

    llvm::BasicBlock* bb = basicBlock();
    for(llvm::Instruction& ins: bb->getInstList()) {
        if (ins.mayReadOrWriteMemory() &&
            !LLVMLoadInstruction::isByvalLoad(&ins)) {
            auto iface = new Interface(this,
                                       LLVMInstruction::GetOutput(&ins),
                                       LLVMInstruction::GetInput(&ins),
                                       false,
                                       llpm::name(&ins) + "_mem");
            _mem[&ins] = iface;
            _function->regBBMemPort(&ins, iface);
        }
    }
}


LLVMControl::LLVMControl(LLVMFunction* func, LLVMBasicBlock* bb) :
    _function(func),
    _basicBlock(bb),
    _bbInput(this, bb->input()->type()),
    _bbOutput(this, bb->output()->type())
{
    if (bb->name() != "")
        this->name(bb->name() + "_control");

    _bbOutputVec = {&_bbOutput};
}

void LLVMControl::construct() {
    llvm::BasicBlock* bb = _basicBlock->basicBlock();
    llvm::TerminatorInst* ti = bb->getTerminator();
    if (ti == NULL) {
        throw InvalidArgument("A basic block does not appear to have a terminator!");
    }

    for (unsigned i=0; i<ti->getNumSuccessors(); i++) {
        llvm::BasicBlock* successor = ti->getSuccessor(i);
        LLVMControl* succControl = _function->getControl(successor);
        assert(succControl != NULL);
        assert(i == _basicBlock->mapSuccessor(successor));

        vector<llvm::Value*> outputValues;
        InputPort* successorPort = succControl->addPredecessor(this, outputValues);

        vector<unsigned>    outputMap;
        vector<llvm::Type*> outputTypes;

        for(auto& v: outputValues) {
            outputTypes.push_back(LLVMBasicBlock::GetHWType(v));
            outputMap.push_back(_basicBlock->mapOutput(v));
        }

        llvm::Type* outType =
            llvm::StructType::get(bb->getContext(), outputTypes);
        _successorMaps.push_back(outputMap);
        _successors.push_back(new OutputPort(this, outType));
        _function->connect(_successors.back(), successorPort);
    }

    if (_basicBlock->returns()) {
        llvm::Value* retValue = _basicBlock->basicBlock()->getTerminator();

        vector<unsigned>    outputMap =
            {_basicBlock->mapOutput(retValue)};
        vector<llvm::Type*> outputTypes = 
            {LLVMBasicBlock::GetHWType(retValue)};

        _successorMaps.push_back(outputMap);

        llvm::Type* outType =
            llvm::StructType::get(bb->getContext(), outputTypes);
        _successors.push_back(new OutputPort(this, outType));
        _function->connectReturn(_successors.back());
    }
}

InputPort* LLVMControl::addPredecessor(LLVMControl* pred, vector<llvm::Value*>& inputData) {
    inputData.clear();
    inputData.resize(_basicBlock->numInputs());

    auto inputMap = _basicBlock->inputMap();
    // printf("\n");
    for (auto pr: inputMap) {
        auto v = pr.first;
        for (auto idx: pr.second) {
            // printf("%u: %s\n", idx, v->getName().str().c_str());

            const std::set<llvm::BasicBlock*>& sources =
                _basicBlock->valueSources(v);
            assert(sources.size() > 0);
            llvm::BasicBlock* predBB =
                (pred == NULL) ?
                    (NULL) :
                    (pred->basicBlock()->basicBlock());
            if (sources.count(predBB) > 0) {
                inputData[idx] = v;
            }
        }
    }

    for (unsigned i=0; i<inputData.size(); i++) {
        assert(inputData[i] != NULL);
    }

    InputPort* ip = new InputPort(this, _bbInput.type());
    _predecessors.push_back(ip);
    return ip;
}

InputPort* LLVMControl::entryPort(vector<llvm::Value*>& inputData) {
    return addPredecessor(NULL, inputData);
}

DependenceRule LLVMControl::depRule(const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    if (op == &_bbInput)
        return DependenceRule(DependenceRule::OR, DependenceRule::Always);
    else
        return DependenceRule(DependenceRule::AND, DependenceRule::Maybe);
}

const std::vector<InputPort*>& LLVMControl::deps(const OutputPort* op) const {
    assert(std::find(outputs().begin(), outputs().end(), op)
           != outputs().end());
    if (op == &_bbInput)
        return _predecessors;
    else
        return _bbOutputVec;
}


llvm::Type* LLVMEntry::FunctionType(llvm::Function* func) {
    vector<llvm::Type*> argTypes;
    auto params = func->getFunctionType();
    if (params->isVarArg())
        throw InvalidArgument("Functions with var args cannot be translated to LLPM");
    for (auto& arg: func->getArgumentList()) {
        argTypes.push_back(LLVMFunction::sanitizeArgumentType(&arg));
    }

    return llvm::StructType::get(func->getContext(), argTypes);
}

std::vector<unsigned> LLVMEntry::ValueMap(llvm::Function* func,
                                          const std::vector<llvm::Value*>& valueMap) {
    map<llvm::Argument*, unsigned> inpMap;
    unsigned ctr = 0;
    for (auto&& arg: func->getArgumentList()) {
        inpMap[&arg] = ctr++;
    }

    std::vector<unsigned> rc;
    for (auto v: valueMap) {
        llvm::Argument* arg = llvm::dyn_cast<llvm::Argument>(v);
        if (arg == NULL)
            throw InvalidArgument("All values in map for LLVMEntry must be arguments!");
        auto f = inpMap.find(arg);
        if (f == inpMap.end())
            throw InvalidArgument("All arguments in map for LLVMEntry must "
                                  "be arguments in local function!");
        rc.push_back(f->second);
    }
    return rc;
}

LLVMFunction::LLVMFunction(llpm::Design& design, llvm::Function* func) :
    ContainerModule(design, func->getName()),
    _call(NULL)
{
    build(func);
}

LLVMFunction::~LLVMFunction() {
}

void LLVMFunction::regBBMemPort(llvm::Value* val, Interface* iface) {
    if (_memInterfaces.count(val) != 0)
        throw InvalidArgument("Can only register one memory port per instruction");
    _memInterfaces[val] =
        addClientInterface(iface->dout(),
                           iface->din(),
                           iface->name());
}

void LLVMFunction::build(llvm::Function* func) {
    // First, we gotta build the blockMap
    for(auto& bb: func->getBasicBlockList()) {
        bool touchesMemory = false;
        for(auto& ins: bb.getInstList()) {
            if (ins.mayReadOrWriteMemory() &&
                !LLVMLoadInstruction::isByvalLoad(&ins))
                touchesMemory = true;
        }

        if (touchesMemory) {
            auto bbBlock = new LLVMImpureBasicBlock(this, &bb);
            _blockMap[&bb] = bbBlock;
        } else {
            auto bbBlock = new LLVMPureBasicBlock(this, &bb);
            _blockMap[&bb] = bbBlock;
        }
    }

    // Second, find all of the requested values
    for(auto& pr: _blockMap) {
        pr.second->buildRequests();
    }

    // Finally, build the I/O types
    for(auto& pr: _blockMap) {
        pr.second->buildIO();
    }

    for(auto& bb: func->getBasicBlockList()) {
        auto bbBlock = _blockMap[&bb];
        auto control = new LLVMControl(this, bbBlock);
        _controlMap[&bb] = control;

        this->connect(&control->_bbInput, bbBlock->input());
        this->connect(&control->_bbOutput, bbBlock->output());
    }

    for(auto cp: _controlMap) {
        auto control = cp.second;
        control->construct();
    }

    assert(_returnPorts.size() > 0);
#if 0
    func->getReturnType()->dump(); printf("\n");
    _returnPorts[0]->type()->dump(); printf("\n");
#endif
    _exit = new LLVMExit(func, _returnPorts.size());
    for (size_t i=0; i<_returnPorts.size(); i++) {
        connect(_returnPorts[i], _exit->din(i));
    }

    llvm::BasicBlock* entryBlock = &func->getEntryBlock();
    LLVMControl* entryControl = _controlMap[entryBlock];
    std::vector<llvm::Value*> entryMap;
    auto entryPort = entryControl->entryPort(entryMap);
    _entry = new LLVMEntry(func, entryMap);

    connect(_entry->dout(), entryPort);

    // Connect input/output
    _call = addServerInterface(_entry->din(), _exit->dout(), "call");
    _call->din()->name("req");
    _call->dout()->name("resp");
}

void LLVMFunction::connectReturn(OutputPort* retPort) {
    _returnPorts.push_back(retPort);
}

} // namespace llpm

