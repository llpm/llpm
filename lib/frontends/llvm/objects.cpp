#include "objects.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>

namespace llpm {

LLVMPureBasicBlock::LLVMPureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb) :
        Function(NULL, NULL),
        LLVMBasicBlock(func, bb)
{
    auto input = buildInputs(bb);
    auto output = buildOutputs(bb);
    this->resetTypes(input, output);
}

llvm::Type* LLVMBasicBlock::buildInputs(llvm::BasicBlock* bb) {
    std::vector<llvm::Type*> inputs;

    // One control signal from each predecessor block
    inputs.push_back(llvm::Type::getVoidTy(bb->getContext()));

    _numInputs = 1;
    BOOST_FOREACH(llvm::Instruction& ins, bb->getInstList()) {
        if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
            // PHI instructions are special
            for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                llvm::Value* v = phi->getIncomingValue(i);
                if (i==0)
                    inputs.push_back(v->getType());
                assert(_inputMap.find(v) == _inputMap.end());
                _inputMap[v] = _numInputs;
            }
            _inputMap[phi] = _numInputs;
            _numInputs += 1;
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                llvm::Value* operand = ins.getOperand(i);
                assert(operand != NULL);
                llvm::Instruction* ins = llvm::dyn_cast<llvm::Instruction>(operand);
                llvm::Argument* arg = llvm::dyn_cast<llvm::Argument>(operand);

                bool isInput = true;
                if (ins != NULL && ins->getParent() == bb)
                    // If it's an instruction and it's local, then
                    // it's not an input!
                    isInput = false;
                if (ins == NULL && arg == NULL)
                    // It's neither an instruction nor an argument,
                    // so it's not an input!
                    isInput = false;

                if (isInput && // It's an input to the basic block
                    _inputMap.find(operand) == _inputMap.end()) { // We haven't seen it already

                    inputs.push_back(operand->getType());
                    _inputMap[operand] = _numInputs;
                    _numInputs += 1;
                }
            }
        }

    }

    return llvm::StructType::create(inputs);
}

llvm::Type* LLVMBasicBlock::buildOutputs(llvm::BasicBlock* bb) {
    std::vector<llvm::Type*> outputs;

    unsigned outputNum = 0;

    // One control signal for each successor block
    llvm::TerminatorInst* ti = bb->getTerminator();
    if (ti) {
        if (llvm::dyn_cast<llvm::ReturnInst>(ti) != NULL) {
            this->_returns = true;
            outputs.push_back(llvm::Type::getVoidTy(bb->getContext()));
            outputNum += 1;
        } else {
            for (unsigned i=0; i<ti->getNumSuccessors(); i++) {
                _successorMap[ti->getSuccessor(i)] = i;
                outputs.push_back(llvm::Type::getVoidTy(bb->getContext()));
                outputNum += 1;
            }
        }
    }

    BOOST_FOREACH(llvm::Instruction& ins, bb->getInstList()) {
        bool usedOutside = false;
        for (auto use_iter = ins.use_begin(); use_iter != ins.use_end(); use_iter++) {
            llvm::User* user = use_iter->getUser();
            llvm::Instruction* userIns = llvm::dyn_cast<llvm::Instruction>(user);
            if (userIns != NULL) {
                if (userIns->getParent() != bb) {
                    usedOutside = true;
                }
            }
        }
        if (usedOutside) {
            outputs.push_back(ins.getType());
            _outputMap[&ins] = outputNum;
        }
    }

    return llvm::StructType::create(outputs);
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

        vector<llvm::Instruction*> outputInsts;
        InputPort* successorPort = succControl->addPredecessor(this, outputInsts);

        vector<unsigned>    outputMap;
        vector<llvm::Type*> outputTypes;
        outputTypes.push_back(llvm::Type::getVoidTy(bb->getContext()));

        BOOST_FOREACH(auto ins, outputInsts) {
            assert(ins->getParent() == bb);
            outputTypes.push_back(ins->getType());
            outputMap.push_back(_basicBlock->mapOutput(ins));
        }

        llvm::Type* outType = llvm::StructType::create(outputTypes);
        successorMaps.push_back(outputMap);
        successors.push_back(new OutputPort(this, outType));
        _function->connect(successors.back(), successorPort);
    }

    if (_basicBlock->returns()) {
        successors.push_back(new OutputPort(this, _basicBlock->output()->type()));
        _function->connectReturn(successors.back());
    }
}

InputPort* LLVMControl::addPredecessor(LLVMControl* pred, vector<llvm::Instruction*>& inputData) {
    inputData.clear();
    vector<unsigned> predMap;
    vector<llvm::Type*> inputTypes = {llvm::Type::getVoidTy(_basicBlock->basicBlock()->getContext())};

    llvm::BasicBlock* bb = _basicBlock->basicBlock();

    BOOST_FOREACH(llvm::Instruction& ins, bb->getInstList()) {
        if (llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&ins)) {
            // PHI instructions are special
            for (unsigned i=0; i<phi->getNumIncomingValues(); i++) {
                llvm::BasicBlock* predBB = phi->getIncomingBlock(i);
                if (predBB == pred->_basicBlock->basicBlock()) {
                    llvm::Value* v = phi->getIncomingValue(i);
                    if (llvm::Instruction* ins = llvm::dyn_cast<llvm::Instruction>(v)) {
                        assert(ins->getParent() == predBB);

                        inputData.push_back(ins);
                        predMap.push_back(_basicBlock->mapInput(ins));
                        inputTypes.push_back(ins->getType());
                    }
                }
            }
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                llvm::Instruction* operand = llvm::dyn_cast<llvm::Instruction>(ins.getOperand(i));
                if (operand != NULL &&
                    operand->getParent() == pred->_basicBlock->basicBlock()) {
                        inputData.push_back(operand);
                        inputTypes.push_back(operand->getType());
                        predMap.push_back(_basicBlock->mapInput(operand));
                }
            }
        }
    }

    InputPort* ip = new InputPort(this, llvm::StructType::create(inputTypes));
    predecessors.push_back(ip);
    return ip;
}

InputPort* LLVMControl::entryPort() {
    vector<unsigned> predMap;
    vector<llvm::Type*> inputTypes = {llvm::Type::getVoidTy(_basicBlock->basicBlock()->getContext())};

    llvm::BasicBlock* bb = _basicBlock->basicBlock();

    BOOST_FOREACH(llvm::Instruction& ins, bb->getInstList()) {
        if (llvm::dyn_cast<llvm::PHINode>(&ins)) {
            // PHI instructions are special
            assert(false && "Entry blocks cannot have PHIs!");
        } else {
            unsigned numOperands = ins.getNumOperands();
            for (unsigned i=0; i<numOperands; i++) {
                llvm::Argument* operand = llvm::dyn_cast<llvm::Argument>(ins.getOperand(i));
                if (operand != NULL ) {
                    inputTypes.push_back(operand->getType());
                    predMap.push_back(_basicBlock->mapInput(operand));
                }
            }
        }
    }

    InputPort* ip = new InputPort(this, llvm::StructType::create(inputTypes));
    predecessors.push_back(ip);
    return ip;
}

LLVMFunction::LLVMFunction(llpm::Design& design, llvm::Function* func) :
    ContainerModule(design, func->getName())
{
    build(func);
}

LLVMFunction::~LLVMFunction() {
}

void LLVMFunction::build(llvm::Function* func) {
    BOOST_FOREACH(auto& bb, func->getBasicBlockList()) {
        bool touchesMemory = false;
        BOOST_FOREACH(auto& ins, bb.getInstList()) {
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

        auto control = new LLVMControl(this, bbBlock);
        _controlMap[&bb] = control;

        this->connect(&control->_bbInput, bbBlock->input());
        this->connect(&control->_bbOutput, bbBlock->output());
    }

    BOOST_FOREACH(auto cp, _controlMap) {
        auto control = cp.second;
        control->construct();
    }

    assert(_returnPorts.size() > 0);
    _exit = new LLVMExit(_returnPorts.size(), _returnPorts[0]->type());
    for (size_t i=0; i<_returnPorts.size(); i++) {
        connect(_returnPorts[i], _exit->din(i));
    }

    vector<llvm::Type*> argTypes = {llvm::Type::getVoidTy(func->getContext())};
    auto params = func->getFunctionType();
    if (params->isVarArg())
        throw InvalidArgument("Functions with var args cannot be translated to LLPM");
    for (unsigned i=0; i<params->getNumParams(); i++) {
        argTypes.push_back(params->getParamType(i));
    }
    _entry = new LLVMEntry(llvm::StructType::create(argTypes));
    llvm::BasicBlock* entryBlock = &func->getEntryBlock();
    LLVMControl* entryControl = _controlMap[entryBlock];
    connect(_entry->dout(), entryControl->entryPort());
}

void LLVMFunction::connectReturn(OutputPort* retPort) {
    _returnPorts.push_back(retPort);
}

} // namespace llpm

