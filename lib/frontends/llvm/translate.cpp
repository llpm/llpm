#include "translate.hpp"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>

#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/LinkAllIR.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>

#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/SubtargetFeature.h>

#include <frontends/llvm/library.hpp>

using namespace std;
using namespace llvm; 

namespace llpm {

LLVMTranslator::LLVMTranslator(Design& design) :
    _design(design) {
    design.refinery().appendLibrary(make_shared<LLVMBaseLibrary>());
}

LLVMTranslator::~LLVMTranslator() {
}

void LLVMTranslator::readBitcode(std::string fileName) {
    setModule(_design.readBitcode(fileName));
}

void LLVMTranslator::optimize(llvm::Module* module) {
    printf("Running LLVM optimizations...\n");
    llvm::legacy::PassManager MPM;

    auto pref = _design.workingDir()->create("preopt.ll");
    raw_os_ostream rawPreStream(pref->openStream());
    MPM.add(createPrintModulePass(rawPreStream));
    MPM.add(llvm::createVerifierPass(true));

    // Add an appropriate DataLayout instance for this module.
    const DataLayout *DL = module->getDataLayout();
    if (DL)
        MPM.add(new DataLayoutPass(module));

    // Analysis
    MPM.add(createTypeBasedAliasAnalysisPass());
    MPM.add(createBasicAliasAnalysisPass());

    MPM.add(createIPSCCPPass());              // IP SCCP
    MPM.add(createGlobalOptimizerPass());     // Optimize out global vars
    MPM.add(createDeadArgEliminationPass());  // Dead argument elimination
    MPM.add(createInstructionCombiningPass());// Clean up after IPCP & DAE

    MPM.add(createCFGSimplificationPass());   // Clean up after IPCP & DAE
    // Start of CallGraph SCC passes.
    MPM.add(createPruneEHPass());             // Remove dead EH info
    MPM.add(createFunctionAttrsPass());       // Set readonly/readnone attrs
    MPM.add(createArgumentPromotionPass());   // Scalarize uninlined fn args

    // Start of function pass.
    // Break up aggregate allocas, using SSAUpdater.
    MPM.add(createSROAPass(/*RequiresDomTree*/ false));
    MPM.add(createEarlyCSEPass());              // Catch trivial redundancies
    MPM.add(createJumpThreadingPass());         // Thread jumps.
    MPM.add(createCorrelatedValuePropagationPass()); // Propagate conditionals
    MPM.add(createCFGSimplificationPass());     // Merge & remove BBs
    MPM.add(createInstructionCombiningPass());  // Combine silly seq's

    MPM.add(createTailCallEliminationPass()); // Eliminate tail calls
    MPM.add(createCFGSimplificationPass());     // Merge & remove BBs
    MPM.add(createReassociatePass());           // Reassociate expressions
    MPM.add(createLoopRotatePass());            // Rotate Loop
    MPM.add(createLICMPass());                  // Hoist loop invariants
    MPM.add(createLoopUnswitchPass());
    MPM.add(createInstructionCombiningPass());
    MPM.add(createIndVarSimplifyPass());        // Canonicalize indvars
    
    MPM.add(createLoopDeletionPass());          // Delete dead loops
    MPM.add(createSimpleLoopUnrollPass());      // Unroll small loops
    MPM.add(createMergedLoadStoreMotionPass()); // Merge load/stores in diamond
    MPM.add(createGVNPass());                   // Remove redundancies
    MPM.add(createSCCPPass());                  // Constant prop with SCCP

    // Run instcombine after redundancy elimination to exploit opportunities
    // opened up by them.
    MPM.add(createInstructionCombiningPass());
    MPM.add(createJumpThreadingPass());         // Thread jumps
    MPM.add(createCorrelatedValuePropagationPass());
    MPM.add(createDeadStoreEliminationPass());  // Delete dead stores

    MPM.add(createLoopRerollPass());
    // MPM.add(createSLPVectorizerPass());  // Vectorize parallel scalar chains.

    // MPM.add(createBBVectorizePass());
    MPM.add(createInstructionCombiningPass());
    MPM.add(createGVNPass());           // Remove redundancies

    // BBVectorize may have significantly shortened a loop body; unroll again.
    MPM.add(createLoopUnrollPass());

    MPM.add(createAggressiveDCEPass());         // Delete dead instructions
    MPM.add(createCFGSimplificationPass()); // Merge & remove BBs
    MPM.add(createInstructionCombiningPass());  // Clean up after everything.

    // FIXME: This is a HACK! The inliner pass above implicitly creates a CGSCC
    // pass manager that we are specifically trying to avoid. To prevent this
    // we must insert a no-op module pass to reset the pass manager.
    // MPM.add(createBarrierNoopPass());
    // MPM.add(createLoopVectorizePass(false, true));
    // FIXME: Because of #pragma vectorize enable, the passes below are always
    // inserted in the pipeline, even when the vectorizer doesn't run (ex. when
    // on -O1 and no #pragma is found). Would be good to have these two passes
    // as function calls, so that we can only pass them when the vectorizer
    // changed the code.
    MPM.add(createInstructionCombiningPass());
    MPM.add(createCFGSimplificationPass());

    MPM.add(createLoopUnrollPass());    // Unroll small loops

    MPM.add(createStripDeadPrototypesPass()); // Get rid of dead prototypes
    MPM.add(createGlobalDCEPass());         // Remove dead fns and globals.
// #endif

    auto postf = _design.workingDir()->create("postopt.ll");
    raw_os_ostream rawPostStream(postf->openStream());
    MPM.add(createPrintModulePass(rawPostStream));
    MPM.add(llvm::createVerifierPass(true));

    MPM.run(*module);
    postf->close();
    pref->close();
}

void LLVMTranslator::setModule(llvm::Module* module) {
    assert(module != NULL);
    optimize(module);
    this->_llvmModule = module;
}

void LLVMTranslator::prepare(llvm::Function* func) {
    if (_toPrepare.count(func) > 0 ||
        _origToPrepared.find(func) != _origToPrepared.end()) {
        // This function already prepared
        return;
    }
    _toPrepare.insert(func);

    // Go through function for calls, prepare them
    for (const BasicBlock& bb: func->getBasicBlockList()) {
        for (const Instruction& ins: bb.getInstList()) {
            if (ins.getOpcode() == Instruction::Call) {
                llvm::Function* f = llvm::dyn_cast<llvm::CallInst>(&ins)->getCalledFunction();
                prepare(f);
                _toPrepare.insert(f);
            }
        }
    }
}

/**
 * If functions have "byval" args, elevate them to actually passing
 * the value without a pointer. Fix the callsites also.
 */
llvm::Function* LLVMTranslator::elevateArgs(llvm::Function* func) {
    /* First determine if there are any arguments to elevate */
    vector<llvm::Type*> argTypes;
    set<unsigned> elevated;
    unsigned i=0;
    for (const Argument& arg: func->getArgumentList()) {
        if (arg.getType()->isPointerTy() && arg.hasByValAttr()) {
            argTypes.push_back(arg.getType()->getPointerElementType());
            elevated.insert(i);
        } else {
            argTypes.push_back(arg.getType());
        }
        i += 1;
    }

    if (elevated.size() == 0) {
        return func;
    }
    // return func;

    // auto origName = func->getName();
    // func->setName(func->getName() + "_orig");


    /* Create a new function with elevated args and a thunk to all the
     * old function */
    llvm::Type* result = func->getReturnType();
    llvm::FunctionType* ft = llvm::FunctionType::get(result, argTypes, func->isVarArg());
    llvm::Function* NF =
        llvm::Function::Create(ft, func->getLinkage(),
                               func->getName() + "_elevated", _llvmModule);
    llvm::IRBuilder<> builder(BasicBlock::Create(func->getContext(), "thunk", NF));
    vector<llvm::Value*> args;
    i=0;
    for (Argument& arg: NF->getArgumentList()) {
        if (elevated.count(i) > 0) {
            auto alloca = builder.CreateAlloca(arg.getType(), NULL);
            builder.CreateStore(&arg, alloca);
            args.push_back(alloca);
        } else {
            args.push_back(&arg);
        }
        i++;
    }
    auto call = llvm::CallInst::Create(func, args, "", builder.GetInsertBlock());
    if (NF->getReturnType()->isVoidTy()) {
        builder.CreateRetVoid();
    } else {
        builder.CreateRet(call);
    }
    llvm::InlineFunctionInfo ifi;
    auto inlined = llvm::InlineFunction(call, ifi);
    assert(inlined);
    printf("Created %s\n", NF->getName().str().c_str());

    /* Fix the old callsites to point to the new guy */
    for (Use& u: func->uses()) {
        llvm::CallSite cs(u.getUser());
        if (!cs)
            continue;
        assert(cs.isCall() && "Only calls (not invokes) are supported right now.");
        llvm::CallInst* origCall = llvm::dyn_cast_or_null<llvm::CallInst>(cs.getInstruction());
        if (origCall->getParent() == builder.GetInsertBlock())
            // Don't replace our thunk call!
            continue;
        if (_toPrepare.count(origCall->getParent()->getParent()) == 0)
            // Don't replace calls in functions we won't be converting
            // to hardware.
            continue;
        assert(origCall != nullptr);

        vector<llvm::Value*> callerArgs;
        for (unsigned i=0; i<origCall->getNumArgOperands(); i++) {
            llvm::Value* arg = origCall->getArgOperand(i);
            if (elevated.count(i) > 0) {
                arg = new llvm::LoadInst(arg, "", origCall);
            }
            callerArgs.push_back(arg);
        }
        llvm::CallInst* newCall = llvm::CallInst::Create(NF, callerArgs, "");
        origCall->replaceAllUsesWith(newCall);
        llvm::ReplaceInstWithInst(origCall, newCall);
    }
    return NF;
}

void LLVMTranslator::prepare(std::string fnName) {
    if (this->_llvmModule == NULL)
        throw InvalidCall("Must load a module into LLVMTranslator before translating");
    llvm::Function* func = this->_llvmModule->getFunction(fnName);
    if (func == NULL)
        throw InvalidArgument("Could not find function: " + fnName);
    prepare(func);
}

void LLVMTranslator::translate() {
    for (llvm::Function* func: _toPrepare) {
        printf("Preparing %s\n", func->getName().str().c_str());
        llvm::Function* newFunc = elevateArgs(func);
        _origToPrepared[func] = newFunc;
        _origToPrepared[newFunc] = newFunc;
    }
    optimize(_llvmModule);
}

LLVMFunction* LLVMTranslator::get(llvm::Function* func) {
    if (func == NULL)
        throw InvalidArgument("Function cannot be NULL!");
    func = _origToPrepared[func];
    if (func == NULL)
        throw InvalidArgument("Function must have been prepared first!");
    return new LLVMFunction(this->_design, this, func);
}

LLVMFunction* LLVMTranslator::get(std::string fnName) {
    if (this->_llvmModule == NULL)
        throw InvalidCall("Must load a module into LLVMTranslator before translating");
    llvm::Function* func = this->_llvmModule->getFunction(fnName);
    if (func == NULL)
        throw InvalidArgument("Could not find function: " + fnName);
    return get(func);
}

} // namespace llpm

