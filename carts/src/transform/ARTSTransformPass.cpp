// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#include "llvm/IR/GlobalVariable.h"
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
#include <cassert>
#include <cstddef>

#include "carts/transform/ARTSTransform.h"

using namespace llvm;
using namespace arts;
// using namespace arts::omp;

/// DEBUG
#define DEBUG_TYPE "arts-transform"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                        ARTSTransformPass                             ///
/// ------------------------------------------------------------------- ///
class ARTSTransformPass : public PassInfoMixin<ARTSTransformPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

PreservedAnalyses ARTSTransformPass::run(Module &M, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n"
                    << TAG << "Running ARTSTransformPass on Module: "
                    << M.getName() << "\n"
                    << "\n-------------------------------------------------\n");

  FunctionAnalysisManager &FAM =
      AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  AnalysisGetter AG(FAM);

  /// Get the set of functions in the module
  SetVector<Function *> Functions;
  for (Function &Fn : M) {
    if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
      continue;
    Functions.insert(&Fn);
  }

  GlobalVariable *GA = M.getGlobalVariable("llvm.global.annotations");
  if (!GA)
    return PreservedAnalyses::all();

  ConstantArray *CA = dyn_cast<ConstantArray>(GA->getOperand(0));
  if (!CA)
    return PreservedAnalyses::all();

  LLVM_DEBUG(dbgs() << TAG << "Found global annotations\n" << *CA << "\n");
  for (auto &Op : CA->operands()) {
    ConstantStruct *CS = dyn_cast<ConstantStruct>(Op);
    if (!CS)
      continue;

    /// Iterate over the operands of the annotation
    /// Argument 0 is the function
    Function *Fn = dyn_cast<Function>(CS->getOperand(0));
    LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName() << "\n");

    /// Argument 1 is the annotation of type private unnamed_addr constant
    GlobalVariable *GV = dyn_cast<GlobalVariable>(CS->getOperand(1));
    if (!GV->hasInitializer()) 
      continue;

    ConstantDataArray *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer());
    if (!CDA)
      continue;

    StringRef Annotation = CDA->getAsCString();
    LLVM_DEBUG(dbgs() << TAG << "Annotation: " << Annotation << "\n");

    /// Parse the annotation: 
    /// - Type of annotation: arts.parallel, arts.task, arts.single...
    size_t Pos = Annotation.find(" ");
    StringRef AnnotationType = Annotation.substr(0, Pos);
    LLVM_DEBUG(dbgs() << TAG << "  - Type: " << AnnotationType << "\n");

    /// Remove the type from the annotation
    if(Pos == std::string::npos)
      continue;
    Annotation = Annotation.substr(Pos + 1);
    LLVM_DEBUG(dbgs() << TAG << "  - Annotation: " << Annotation << "\n");

    /// Sepparate for comma or end of string
    /// Get deps(in: )
    if (Annotation.starts_with("deps(in: ")) {
      Annotation = Annotation.substr(9);

      Pos = Annotation.find(")");
      StringRef Deps = Annotation.substr(0, Pos);
      LLVM_DEBUG(dbgs() << TAG << "  - Found depend(in: " << Deps << ")\n");
    }

    Annotation = Annotation.substr(Pos + 2);
    LLVM_DEBUG(dbgs() << TAG << "  - Annotation: " << Annotation << "\n");
    if(Annotation.starts_with("deps(out: ")) {
      Annotation = Annotation.substr(10);

      Pos = Annotation.find(")");
      StringRef Deps = Annotation.substr(0, Pos);
      LLVM_DEBUG(dbgs() << TAG << "  - Found depend(out: " << Deps << ")\n");
    }
  }

  /// Run the visitor
  // OMPVisitor OV(M, Functions);
  // OV.run(InputASTFilename);

  /// Run the transform
  // ARTSTransform OT(M, AG);
  // OT.run(AM);

  // /// Create attributor
  // CallGraphUpdater CGUpdater;
  // BumpPtrAllocator Allocator;
  // InformationCache InfoCache(M, AG, Allocator, &Functions);
  // AttributorConfig AC(CGUpdater);
  // AC.DefaultInitializeLiveInternals = false;
  // AC.IsModulePass = true;
  // AC.RewriteSignatures = false;
  // AC.MaxFixpointIterations = 32;
  // AC.DeleteFns = false;
  // AC.PassName = DEBUG_TYPE;
  // Attributor A(Functions, InfoCache, AC);
  // /// Register attributes for functions
  // for (Function *F : Functions) {
  //   for (Argument &Arg : F->args())
  //     A.getOrCreateAAFor<AAMemoryBehavior>(IRPosition::argument(Arg));
  // }

  // ChangeStatus Changed = A.run();
  // LLVM_DEBUG(dbgs() << TAG << "[Attributor] Done with " << Functions.size()
  //                   << " functions, result: " << Changed << ".\n");

  /// Print module info
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n"
                    << TAG << "OmpTransformPass has finished\n\n"
                    << M << "\n"
                    << "\n-------------------------------------------------\n");
  return PreservedAnalyses::all();
}

// This part is the new way of registering your pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ARTSTransform", "v0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "arts-transform") {
                    FPM.addPass(ARTSTransformPass());
                    return true;
                  }
                  return false;
                });
          }};
}