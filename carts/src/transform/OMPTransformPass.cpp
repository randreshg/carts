// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/IPO/Attributor.h"

#include "llvm/Support/Debug.h"

#include "carts/transform/OMPTransform.h"
#include "carts/utils/ARTSUtils.h"

using namespace llvm;
using namespace arts;
using namespace arts::omp;
using namespace arts::utils;

/// DEBUG
#define DEBUG_TYPE "omp-transform"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                        OMPTransformPass                             ///
/// ------------------------------------------------------------------- ///
class OMPTransformPass : public PassInfoMixin<OMPTransformPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

PreservedAnalyses OMPTransformPass::run(Module &M, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Running OmpTransformPass on Module: \n"
                    << M.getName() << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  FunctionAnalysisManager &FAM =
      AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  AnalysisGetter AG(FAM);
  OMPTransform OT(M, AG);
  OT.run(AM);
  /// Get the set of functions in the module
  SetVector<Function *> Functions;
  for (Function &Fn : M) {
    if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
      continue;
    Functions.insert(&Fn);
  }
  /// Create attributor
  CallGraphUpdater CGUpdater;
  BumpPtrAllocator Allocator;
  InformationCache InfoCache(M, AG, Allocator, &Functions);
  AttributorConfig AC(CGUpdater);
  AC.DefaultInitializeLiveInternals = false;
  AC.IsModulePass = true;
  AC.RewriteSignatures = false;
  AC.MaxFixpointIterations = 32;
  AC.DeleteFns = false;
  AC.PassName = DEBUG_TYPE;
  Attributor A(Functions, InfoCache, AC);
  /// Register attributes for functions
  for (Function *F : Functions) {
    for (Argument &Arg : F->args())
      A.getOrCreateAAFor<AAMemoryBehavior>(IRPosition::argument(Arg));
  }

  ChangeStatus Changed = A.run();
  LLVM_DEBUG(dbgs() << TAG << "[Attributor] Done with " << Functions.size()
                    << " functions, result: " << Changed << ".\n");

  /// Print module info
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "OmpTransformPass has finished\n\n" << M << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return PreservedAnalyses::all();
}

// This part is the new way of registering your pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "OMPTransform", "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "omp-transform") {
                    FPM.addPass(OMPTransformPass());
                    return true;
                  }
                  return false;
                });
          }};
}