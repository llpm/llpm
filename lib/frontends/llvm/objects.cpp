#include "objects.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
#include <llvm/Support/Casting.h>

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
    llvm::Instruction* operand = llvm::dyn_cast<llvm::Instruction>(v);
    bool nonLocal = 
        (operand && operand->getParent() != bb);
    return !nonLocal;
}

static void checkValue(llvm::Value* operand) {
    if (llvm::dyn_cast<llvm::Argument>(operand) ||
        llvm::dyn_cast<llvm::Instruction>(operand) ||
        llvm::dyn_cast<llvm::Constant>(operand))
        return;

    operand->dump();
    throw InvalidArgument("I don't know how to deal with value type!");
}

static bool skipOperand(llvm::Instruction& i, unsigned idx) {
    switch(i.getOpcode()) {
    case llvm::Instruction::Br:
        if (llvm::dyn_cast<llvm::BranchInst>(&i)->isConditional())
            return idx > 0;
        return true;
    default:
        return false;
    }
}

LLVMBasicBlock::LLVMBasicBlock(LLVMFunction* f, llvm::BasicBlock* bb):
    _function(f),
    _basicBlock(bb),
    _returns(false),
    _numInputs(0),
    _numOutputs(0)
{
    llvm::TerminatorInst* ti = _basicBlock->getTerminator();
    if (ti == NULL) {
        throw InvalidArgument("A basic block does not appear to have a terminator!");
    }

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

void LLVMBasicBlock::buildRequests() {
    std::set<llvm::BasicBlock*> predecessors(llvm::pred_begin(_basicBlock), llvm::pred_end(_basicBlock));

    std::vector<llvm::Type*> inputTypes;

    for(llvm::Instruction& ins: _basicBlock->getInstList()) {
        if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
            // PHI instructions are special
            inputTypes.push_back(phi->getType());
            assert(phi->getNumIncomingValues() > 0);
            for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                llvm::Value* v = phi->getIncomingValue(i);
                llvm::BasicBlock* pred = phi->getIncomingBlock(i);
                assert(_inputMap.find(v) == _inputMap.end());
                _inputMap[v] = _numInputs;
                _valueSources[v].insert(pred);

                auto predBlock = _function->blockMap(pred);
                assert(predBlock);
                predBlock->requestOutput(v);
            }
            _numInputs += 1;
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                if (skipOperand(ins, i))
                    continue;

                llvm::Value* operand = ins.getOperand(i);
                assert(operand != NULL);
                checkValue(operand);

                if (!isLocalOrConst(_basicBlock, operand)) {
                    addInput(operand);
                    for(auto pred: predecessors) {
                        auto predBlock = _function->blockMap(pred);
                        assert(predBlock);
                        predBlock->requestOutput(operand);
                        _valueSources[operand].insert(pred);
                    }

                    if (isLocalArg(_basicBlock, operand))
                        _valueSources[operand].insert(NULL);
                    else if (predecessors.size() == 0) {
                        _basicBlock->dump();
                        operand->dump();
                        assert(predecessors.size() > 0);
                    }
                }
            }
        }
    }
}

void LLVMBasicBlock::requestOutput(llvm::Value* v) {
    if (_outputMap.find(v) != _outputMap.end())
        return;

    addOutput(v);
    if (!isLocalInstruction(_basicBlock, v)) {
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

void LLVMBasicBlock::buildIO() {
    
    /**
     * Build Input Array
     */
    std::vector<llvm::Type*> inputs(_numInputs);

    // One control signal from each predecessor block
    // printf("\n");
    for (auto& pr: _inputMap) {
        auto value = pr.first;
        auto idx = pr.second;

#if 0
        printf("%u: %s\n",
               idx,
               value->getName().str().c_str());
#endif

        if (inputs[idx] != NULL) {
            assert(inputs[idx] == value->getType());
        } else {
            inputs[idx] = value->getType();
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

    // One control signal from each predecessor block
    for (auto& pr: _outputMap) {
        auto value = pr.first;
        auto idx = pr.second;

        if (outputs[idx] != NULL) {
            assert(outputs[idx] == value->getType());
        } else {
            outputs[idx] = value->getType();
        }
    }

    for (auto& ot: outputs) {
        assert(ot != NULL);
    }

    auto output = llvm::StructType::get(_basicBlock->getContext(), outputs);

    // Reset I/O types
    this->resetTypes(input, output);
}


LLVMControl::LLVMControl(LLVMFunction* func, LLVMBasicBlock* bb) :
    _function(func),
    _basicBlock(bb),
    _bbInput(this, bb->input()->type()),
    _bbOutput(this, bb->output()->type())
{  }

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
            outputTypes.push_back(v->getType());
            outputMap.push_back(_basicBlock->mapOutput(v));
        }

        llvm::Type* outType = llvm::StructType::get(bb->getContext(), outputTypes);
        _successorMaps.push_back(outputMap);
        _successors.push_back(new OutputPort(this, outType));
        _function->connect(_successors.back(), successorPort);
    }

    if (_basicBlock->returns()) {
        _successors.push_back(new OutputPort(this, _basicBlock->output()->type()));
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
        auto idx = pr.second;
        // printf("%u: %s\n", idx, v->getName().str().c_str());

        const std::set<llvm::BasicBlock*>& sources = _basicBlock->valueSources(v);
        assert(sources.size() > 0);
        llvm::BasicBlock* predBB =
            (pred == NULL) ?
                (NULL) :
                (pred->basicBlock()->basicBlock());
        if (sources.count(predBB) > 0) {
            inputData[idx] = v;
        }
    }

#if 0
    for(llvm::Instruction& ins: bb->getInstList()) {
        // Verify all inputs come from predecessor


        unsigned numOperands = ins.getNumOperands();
        for (unsigned i=0; i<numOperands; i++) {
            auto operand = ins.getOperand(i);
            llvm::Constant* c = llvm::dyn_cast<llvm::Constant>(operand);
            if (c != NULL && inputMap.find(operand) == inputMap.end()) {
                throw ImplementationError("All LLVM basic blocks being translated must take "
                                          "all of their inputs from their input map!");
            }
        }

        if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
            if (pred == NULL) {
                // NULL pred means we are building entry port!
                throw new InvalidArgument("Entry blocks cannot have PHI nodes!");
            }

            // PHI instructions are special
            for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                llvm::BasicBlock* predBB = phi->getIncomingBlock(i);
                if (predBB == pred->_basicBlock->basicBlock()) {
                    llvm::Value* v = phi->getIncomingValue(i);
                    unsigned idx = _basicBlock->mapInput(v);
                    inputData[idx] = v;
                }
            }
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                llvm::Value* operand = ins.getOperand(i);
                if (!isLocalOrConst(bb, operand)) {
                    auto idx = _basicBlock->mapInput(operand);
                    inputData[idx] = operand;
                }
            }
        }
    }
#endif

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

llvm::Type* LLVMEntry::FunctionType(llvm::Function* func) {
    vector<llvm::Type*> argTypes;
    auto params = func->getFunctionType();
    if (params->isVarArg())
        throw InvalidArgument("Functions with var args cannot be translated to LLPM");
    for (unsigned i=0; i<params->getNumParams(); i++) {
        argTypes.push_back(params->getParamType(i));
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
    ContainerModule(design, func->getName())
{
    build(func);
}

LLVMFunction::~LLVMFunction() {
}

void LLVMFunction::build(llvm::Function* func) {
    // First, we gotta build the blockMap
    for(auto& bb: func->getBasicBlockList()) {
        bool touchesMemory = false;
        for(auto& ins: bb.getInstList()) {
            if (ins.mayReadOrWriteMemory())
                touchesMemory = true;
        }

        LLVMPureBasicBlock* bbBlock;
        if (touchesMemory) {
            throw InvalidArgument("loads/stores not yet supported!");
        } else {
            bbBlock = new LLVMPureBasicBlock(this, &bb);
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
    _exit = new LLVMExit(_returnPorts.size(), _returnPorts[0]->type());
    for (size_t i=0; i<_returnPorts.size(); i++) {
        connect(_returnPorts[i], _exit->din(i));
    }

    llvm::BasicBlock* entryBlock = &func->getEntryBlock();
    LLVMControl* entryControl = _controlMap[entryBlock];
    std::vector<llvm::Value*> entryMap;
    auto entryPort = entryControl->entryPort(entryMap);
    _entry = new LLVMEntry(func, entryMap);

    connect(_entry->dout(), entryPort);
}

void LLVMFunction::connectReturn(OutputPort* retPort) {
    _returnPorts.push_back(retPort);
}


} // namespace llpm

