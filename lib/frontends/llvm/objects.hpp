#ifndef __LLPM_LLVM_OBJECTS_HPP__
#define __LLPM_LLVM_OBJECTS_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <libraries/core/interface.hpp>
#include <libraries/util/types.hpp>

namespace llvm {
    class PHINode;
    class CallInst;
}

namespace llpm {

// Fwd defs. C++, you so silly!
class LLVMBasicBlock;
class LLVMControl;
class LLVMTranslator;

class LLVMEntry: public StructTwiddler {
    static llvm::Type* FunctionType(llvm::Function*);
    static std::vector<unsigned> ValueMap(llvm::Function*, const std::vector<llvm::Value*>&);
public:
    LLVMEntry(llvm::Function* func, const std::vector<llvm::Value*>& map);
    virtual ~LLVMEntry() { }
};

class LLVMExit: public Select {
public:
    LLVMExit(llvm::Function* func, unsigned N);
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

    Interface* _call;
    LLVMTranslator* _translator;

    // Memory load request ports
    std::map<llvm::Value*, Interface*> _memInterfaces;
    std::map<llvm::CallInst*, Interface*> _callInterfaces;

    LLVMFunction(Design&, LLVMTranslator*, llvm::Function*);
    void build(llvm::Function* func);

public:
    virtual ~LLVMFunction();


    static llvm::Type* sanitizeType(llvm::Value* val);
    static llvm::Type* sanitizeArgumentType(llvm::Argument* arg);

    DEF_GET_NP(call);
    DEF_GET_NP(translator);

    const std::map<llvm::Value*, Interface*>& mem() const {
        return _memInterfaces;
    }

    const std::map<llvm::CallInst*, Interface*>& calls() const {
        return _callInterfaces;
    }

    LLVMControl* getControl(llvm::BasicBlock* bb) const {
        auto f = _controlMap.find(bb);
        if (f == _controlMap.end())
            return NULL;
        else
            return f->second;
    }

    void connectReturn(OutputPort* retPort);
    LLVMBasicBlock* blockMap(llvm::BasicBlock* bb) const {
        auto f = _blockMap.find(bb);
        if (f == _blockMap.end())
            return NULL;
        return f->second;
    }

    void regBBMemPort(llvm::Value*, Interface*);
    void regBBCallPort(llvm::CallInst*, Interface*);
};



class LLVMBasicBlock: public virtual Block {
    friend class LLVMFunction;
protected:
    LLVMFunction* _function;
    llvm::BasicBlock* _basicBlock;

    std::map<llvm::PHINode*, unsigned> _phiInputMap;
    std::map<llvm::Value*, unsigned> _nonPhiInputMap;

    std::map<llvm::Value*, std::set<llvm::BasicBlock*> > _valueSources;
    std::map<llvm::Value*, unsigned> _outputMap;
    std::map<llvm::BasicBlock*, unsigned> _successorMap;
    std::set<llvm::Value*> _passthroughs;
    std::set<llvm::Constant*> _constants;

    bool _returns;
    unsigned _numInputs;
    unsigned _numOutputs;

    void addInput(llvm::Value* v);

    void addOutput(llvm::Value* v) {
        if (_outputMap.find(v) != _outputMap.end())
            return;
        _outputMap[v] = _numOutputs++;
    }

    LLVMBasicBlock(LLVMFunction* f, llvm::BasicBlock* bb);

    void requestOutput(llvm::Value*);

    virtual void resetTypes(llvm::Type* input, llvm::Type* output) = 0;

public:
    virtual ~LLVMBasicBlock() { }

    static llvm::Type* GetHWType(llvm::Value*);
    virtual void buildRequests();
    virtual void buildIO();

    DEF_GET_NP(function);
    DEF_GET_NP(basicBlock);
    DEF_GET_NP(returns);
    DEF_GET_NP(numInputs);
    DEF_GET_NP(numOutputs);
    DEF_GET_NP(passthroughs);
    DEF_GET_NP(constants);

    virtual InputPort* input() = 0;
    virtual const InputPort* input() const = 0;
    virtual OutputPort* output() = 0;
    virtual const OutputPort* output() const = 0;

    const std::set<llvm::BasicBlock*>& valueSources(
        llvm::Value* v) const
    {
        static const std::set<llvm::BasicBlock*> emptySet;
        auto f = _valueSources.find(v);
        if (f == _valueSources.end())
            return emptySet;
        return f->second;
    }

    const std::map<llvm::PHINode*, unsigned>& phiInputMap() const {
        return _phiInputMap;
    }

    const std::map<llvm::Value*, unsigned>& nonPhiInputMap() const {
        return _nonPhiInputMap;
    }

    unsigned mapOutput(llvm::Value* ins) const {
        auto f = _outputMap.find(ins);
        assert(f != _outputMap.end());
        return f->second;
    }

    const std::map<llvm::Value*, unsigned>& outputMap() const {
        return _outputMap;
    }

    unsigned mapSuccessor(llvm::BasicBlock* bb) const {
        auto f = _successorMap.find(bb);
        assert(f != _successorMap.end());
        return f->second;
    }

    virtual Interface* mem(llvm::Value*) const {
        return NULL;
    }

    virtual Interface* call(llvm::Instruction*) const {
        return NULL;
    }
};

// Pure LLVM Basic Blocks (those without loads/stores) are treated
// as functions with a single input and a single output and no
// state.
class LLVMPureBasicBlock: public Function, public LLVMBasicBlock {
    friend class LLVMFunction;

    LLVMPureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb);

public:
    virtual ~LLVMPureBasicBlock() { }

    virtual InputPort* input() {
        return &_din;
    }
    virtual const InputPort* input() const {
        return &_din;
    }
    virtual OutputPort* output() {
        return &_dout;
    }
    virtual const OutputPort* output() const {
        return &_dout;
    }

    virtual void resetTypes(llvm::Type* input, llvm::Type* output) {
        Function::resetTypes(input, output);
    }

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND_FireOne, inputs());
    }
};

// LLVM Basic Blocks with loads/stores
class LLVMImpureBasicBlock: public LLVMBasicBlock {
    friend class LLVMFunction;

    // SSA inputs and outputs
    InputPort _din;
    OutputPort _dout;

    // Memory load request ports
    std::map<llvm::Value*, std::unique_ptr<Interface>> _mem;

    // Function call request ports
    std::map<llvm::CallInst*, std::unique_ptr<Interface>> _call;

    LLVMImpureBasicBlock(LLVMFunction* func, llvm::BasicBlock* bb);

public:
    virtual ~LLVMImpureBasicBlock() { }

    virtual void buildIO();

    virtual InputPort* input() {
        return &_din;
    }
    virtual const InputPort* input() const {
        return &_din;
    }
    virtual OutputPort* output() {
        return &_dout;
    }
    virtual const OutputPort* output() const {
        return &_dout;
    }

    virtual bool hasState() const {
        return false;
    }

    virtual void resetTypes(llvm::Type* input, llvm::Type* output) {
        _din.reset(input);
        _dout.reset(output);
    }

    virtual Interface* mem(llvm::Value* v) const {
        auto f = _mem.find(v);
        if (f == _mem.end())
            return NULL; 
        return f->second.get();
    }

    virtual Interface* call(llvm::Instruction* ins) const;

    virtual DependenceRule deps(const OutputPort* op) const;
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
    std::vector<InputPort*> _bbOutputVec;

    std::vector<InputPort*>  _predecessors;
    std::vector<OutputPort*> _successors;
    std::vector< std::vector<unsigned> > _successorMaps;

    LLVMControl(LLVMFunction* func, LLVMBasicBlock* basicBlock);

public:
    virtual ~LLVMControl() { 
        DEL_ARRAY(_predecessors);
        DEL_ARRAY(_successors);
    }

    DEF_GET_NP(function);
    DEF_GET_NP(basicBlock);

    DEF_ARRAY_GET(predecessors);
    DEF_ARRAY_GET(successors);
    DEF_ARRAY_GET(successorMaps);

    DEF_GET(bbInput);
    DEF_GET(bbOutput);

    virtual bool hasState() const {
        return false;
    }

    void construct();
    InputPort* addPredecessor(LLVMControl* pred, std::vector<llvm::Value*>& inputData);
    InputPort* entryPort(std::vector<llvm::Value*>& inputData);

    virtual DependenceRule deps(const OutputPort* op) const;
};

} // namespace llpm

#endif // __LLPM_LLVM_OBJECTS_HPP__
