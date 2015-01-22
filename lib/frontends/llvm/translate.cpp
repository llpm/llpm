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
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/LinkAllIR.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/raw_os_ostream.h>

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



void LLVMTranslator::setModule(llvm::Module* module) {
    assert(module != NULL);

    llvm::legacy::PassManager MPM;

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


    auto f = _design.workingDir()->create("postopt.ll");
    raw_os_ostream rawStream(f->openStream());
    MPM.add(createPrintModulePass(rawStream));
    MPM.add(llvm::createVerifierPass(true));

    MPM.run(*module);
    f->close();

    this->_llvmModule = module;
}

LLVMFunction* LLVMTranslator::translate(llvm::Function* func) {
    if (func == NULL)
        throw InvalidArgument("Function cannot be NULL!");
    return new LLVMFunction(this->_design, func);
}

LLVMFunction* LLVMTranslator::translate(std::string fnName) {
    if (this->_llvmModule == NULL)
        throw InvalidCall("Must load a module into LLVMTranslator before translating");
    llvm::Function* func = this->_llvmModule->getFunction(fnName);
    if (func == NULL)
        throw InvalidArgument("Could not find function: " + fnName);
    return translate(func);
}

} // namespace llpm

