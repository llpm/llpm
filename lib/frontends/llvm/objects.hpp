#ifndef __LLPM_LLVM_OBJECTS_HPP__
#define __LLPM_LLVM_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

// Fwd defs. C++, you so silly!
class LLVMBasicBlock;
class LLVMControl;

class LLVMEntry: public Identity {
public:
    LLVMEntry(llvm::Type* type) :
        Identity(type) { }
    virtual ~LLVMEntry() { }
};

class LLVMExit: public Select {
public:
    LLVMExit(unsigned N, llvm::Type* type) :
        Select(N, type) { }
    virtual ~LLVMExit() { }
};

// Represents a function in LLVM. Not to be confused with our
// (LLPM's) definition of a function -- a block with one input, one
// output and no state
class LLVMFunction: public ContainerModule {
    friend class LLVMTranslator;

    std::map<llvm::BasicBlock*, LLVMBasicBlock*> _blockMap;
    std::map<llvm::BasicBlock*, LLVMControl*> _controlMap;
    std::vector<OutputPort*> _returnPorts;
    LLVMEntry* _entry;
    LLVMExit* _exit;

    LLVMFunction(Design&, llvm::Function*);
    void build(llvm::Function* func);

public:
    virtual ~LLVMFunction();

    LLVMControl* getControl(llvm::BasicBlock* bb) const {
        auto f = _controlMap.find(bb);
        if (f == _controlMap.end())
            return NULL;
        else
            return f->second;
    }

    void connectReturn(OutputPort* retPort);
};



class LLVMBasicBlock {
protected:
    LLVMFunction* _function;
    llvm::BasicBlock* _basicBlock;
    std::map<llvm::Value*, unsigned> _inputMap;
    std::map<llvm::Value*, unsigned> _outputMap;
    std::map<llvm::BasicBlock*, unsigned> _successorMap;
    bool _returns;
    unsigned _numInputs;

    LLVMBasicBlock(LLVMFunction* f, llvm::BasicBlock* bb):
        _function(f),
        _basicBlock(bb),
        _returns(false),
        _numInputs(0)
    { }

    llvm::Type* buildInputs(llvm::BasicBlock* bb);
    llvm::Type* buildOutputs(llvm::BasicBlock* bb);

public:
    virtual ~LLVMBasicBlock() { }

    DEF_GET_NP(function);
    DEF_GET_NP(basicBlock);
    DEF_GET_NP(returns);
    DEF_GET_NP(numInputs);

    virtual InputPort* input() = 0;
    virtual OutputPort* output() = 0;

    unsigned mapInput(llvm::Value* ins) {
        auto f = _inputMap.find(ins);
        assert(f != _inputMap.end());
        return f->second;
    }

    unsigned mapOutput(llvm::Instruction* ins) {
        auto f = _outputMap.find(ins);
        assert(f != _outputMap.end());
        return f->second;
    }

    unsigned mapSuccessor(llvm::BasicBlock* bb) {
        auto f = _successorMap.find(bb);
        assert(f != _successorMap.end());
        return f->second;
    }
};

// Pure LLVM Basic Blocks (those without loads/stores) are treated
// as functions with a single input and a single output and no
// state.
class LLVMPureBasicBlock: public Function, public LLVMBasicBlock {
    friend class LLVMFunction;

    LLVMPureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb);
    virtual ~LLVMPureBasicBlock() { }

    virtual InputPort* input() {
        return &_din;
    }
    virtual OutputPort* output() {
        return &_dout;
    }
};

// LLVMBasicBlocks are connected via LLVMControl blocks. Separating
// the control part of the basic block from the dataflow part of the
// basic block allows subsequent passes to refine them in different
// manners
class LLVMControl: public Block {
    friend class LLVMFunction;
    LLVMFunction* _function;
    LLVMBasicBlock* _basicBlock;

    OutputPort _bbInput;
    InputPort _bbOutput;

    std::vector<InputPort*>  predecessors;
    std::vector< std::vector<unsigned> > predecessorMaps;
    std::vector<OutputPort*> successors;
    std::vector< std::vector<unsigned> > successorMaps;

    LLVMControl(LLVMFunction* func, LLVMBasicBlock* basicBlock);

public:
    virtual ~LLVMControl() { }

    DEF_GET_NP(function);
    DEF_GET_NP(basicBlock);

    virtual bool hasState() const {
        return false;
    }

    void construct();
    InputPort* addPredecessor(LLVMControl* pred, vector<llvm::Instruction*>& inputData);
    InputPort* entryPort();
};

} // namespace llpm

#endif // __LLPM_LLVM_OBJECTS_HPP__
