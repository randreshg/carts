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
#include "carts/transform/visitor/OMPVisitor.h"

using namespace llvm;
using namespace arts;
using namespace arts::omp;

// Command-line option to specify the input AST file
static cl::opt<std::string>
    InputASTFilename("ast-file", cl::desc("Specify the input AST filename"),
                     cl::value_desc("filename"),
                     cl::init("input.ast")); // Default value if not specified

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

  void analyzeDependencies(Loop &L, ScalarEvolution &SE) {
    errs() << "Analyzing dependencies...\n";
    for (auto *BB : L.blocks()) {
      for (auto &I : *BB) {
        if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
          const SCEV *S = SE.getSCEV(GEP);
          errs() << "GEP Access Pattern: ";
          S->print(errs());
          errs() << "\n";
        } else if (auto *Load = dyn_cast<LoadInst>(&I)) {
          const SCEV *S = SE.getSCEV(Load->getPointerOperand());
          errs() << "Load Access Pattern: ";
          S->print(errs());
          errs() << "\n";
        } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
          const SCEV *S = SE.getSCEV(Store->getPointerOperand());
          errs() << "Store Access Pattern: ";
          S->print(errs());
          errs() << "\n";
        }
      }
    }
  }

  // void analyzeLoop(Loop &L, ScalarEvolution &SE) {
  //   errs() << "Loop at depth " << L.getLoopDepth() << "\n";

  //   /// 1. Get induction variable
  //   if (auto *IV = L.getInductionVariable(SE)) {
  //     const SCEV *IndVarExpr = SE.getSCEV(IV);
  //     errs() << "Induction Variable: ";
  //     IndVarExpr->print(errs());
  //     errs() << "\n";
  //   } else {
  //     errs() << "No induction variable found.\n";
  //   }

  //   // 2. Get loop bounds
  //   const SCEV *BECount = SE.getBackedgeTakenCount(&L);
  //   if(!BECount) {
  //     errs() << "Backedge-taken count not computable.\n";
  //     return;
  //   }
  //   if (SE.isSCEVable(BECount->getType())) {
  //     if (auto *BEAddRec = dyn_cast<SCEVAddRecExpr>(BECount)) {
  //       const SCEV *Start = BEAddRec->getStart();
  //       const SCEV *End = BEAddRec->getStepRecurrence(SE);
  //       errs() << "Loop Start: ";
  //       Start->print(errs());
  //       errs() << "\nLoop End: ";
  //       End->print(errs());
  //       errs() << "\n";
  //     } else {
  //       errs() << "Loop bounds could not be determined symbolically.\n";
  //     }
  //   } else {
  //     errs() << "Backedge-taken count not computable.\n";
  //   }

  //   // 3. Analyze memory dependencies
  //   analyzeDependencies(L, SE);
  // }

  void analyzeLoop(Loop &L, ScalarEvolution &SE) {
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - -\n");
    errs() << "Loop at depth " << L.getLoopDepth() << "\n";

    /// DEbug loop
    L.dump();

    if (!L.isLoopSimplifyForm()) {
      errs() << "Loop is not in simplified form.\n";
    }

    auto *Predecessor = L.getLoopPredecessor();
    if (Predecessor) {
      errs() << "Predecessor: " << *Predecessor << "\n";
    } else {
      errs() << "No predecessor found.\n";
    }

    auto *PreHeader = L.getLoopPreheader();
    if (PreHeader) {
      errs() << "Preheader: " << *PreHeader << "\n";
    } else {
      errs() << "No preheader found.\n";
    }

    auto *Latch = L.getLoopLatch();
    if (Latch) {
      errs() << "Latch: " << *Latch << "\n";
    } else {
      errs() << "No latch found.\n";
    }

    if (L.hasDedicatedExits())
      errs() << "Loop has dedicated exits.\n";
    else
      errs() << "Loop does not have dedicated exits.\n";

    // Try to get induction variable using Scalar Evolution
    if (auto *IV = L.getInductionVariable(SE)) {
      const SCEV *IndVarExpr = SE.getSCEV(IV);
      errs() << "Induction Variable (Scalar Evolution): ";
      IndVarExpr->print(errs());
      errs() << "\n";
    } else {
      errs() << "No induction variable found by Scalar Evolution. Attempting "
                "manual detection.\n";
      detectInductionVariableManually(L);
    }

    // Analyze loop bounds
    const SCEV *BECount = SE.getBackedgeTakenCount(&L);
    if (BECount && !isa<SCEVCouldNotCompute>(BECount)) {
      errs() << "Backedge-taken count: ";
      BECount->print(errs());
      errs() << "\n";
    } else {
      errs() << "Backedge-taken count not computable.\n";
    }

    analyzeDependencies(L, SE);
  }

  void detectInductionVariableManually(Loop &L) {
    PHINode *IndVar = nullptr;
    for (auto &PHI : L.getHeader()->phis()) {
      // Check if the PHI node has two incoming values
      if (PHI.getNumIncomingValues() != 2)
        continue;

      // Check incoming edges (preheader and back edge)
      Value *PreHeaderVal = PHI.getIncomingValueForBlock(L.getLoopPreheader());
      Value *BackEdgeVal = PHI.getIncomingValueForBlock(L.getLoopLatch());

      // Ensure the back-edge value is an increment of the preheader value
      if (auto *BinOp = dyn_cast<BinaryOperator>(BackEdgeVal)) {
        if (BinOp->getOpcode() == Instruction::Add ||
            BinOp->getOpcode() == Instruction::Sub) {
          if (BinOp->getOperand(0) == &PHI || BinOp->getOperand(1) == &PHI) {
            IndVar = &PHI;
            break;
          }
        }
      }
    }

    if (IndVar) {
      errs() << "Detected Induction Variable (Manual): " << *IndVar << "\n";
    } else {
      errs() << "Failed to detect induction variable manually.\n";
    }
  }
};

PreservedAnalyses OMPTransformPass::run(Module &M, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n"
                    << TAG << "Running OmpTransformPass on Module: "
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

    /// Analyze loops in the function
    auto &SE = *AG.getAnalysis<ScalarEvolutionAnalysis>(Fn);
    auto &LI = *AG.getAnalysis<LoopAnalysis>(Fn);
    // auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
    //   auto &LI = FAM.getResult<LoopAnalysis>(F);
    LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - \n");
    LLVM_DEBUG(dbgs() << "Analyzing loops in function: " << Fn.getName()
                      << "\n");
    for (auto *Loop : LI) {
      analyzeLoop(*Loop, SE);
    }
  }

  /// Run the visitor
  OMPVisitor OV(M, Functions);
  OV.run(InputASTFilename);

  // /// Run the transform
  // OMPTransform OT(M, AG, OV);
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