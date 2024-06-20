// Description: Main file for the ARTS Analysis pass.
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/MemorySSA.h"
// #include "llvm/Analysis/MemorySSAWrapperPass.h"
#include "llvm/IR/LegacyPassManager.h"
// #include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Debug.h"

// #include "carts/utils/ARTS.h"
#include "carts/analysis/graph/EDTGraph.h"
#include "noelle/core/Noelle.hpp"
#include "llvm/Analysis/MemorySSA.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "arts-analysis"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                        ARTSAnalysisPass                             ///
/// ------------------------------------------------------------------- ///

namespace {
using namespace arcana::noelle;

struct ARTSAnalysisPass : public ModulePass {
  static char ID;

  ARTSAnalysisPass() : ModulePass(ID) {}

  bool doInitialization(Module &M) override { return false; }

  bool runOnModule(Module &M) override {
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Running ARTS Analysis pass on Module: \n"
                      << M.getName() << "\n");
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n");
    /// Fetch NOELLE Manager
    auto &NM = getAnalysis<Noelle>();
    Function *F = M.getFunction("main");
    auto &MSSA = getAnalysis<MemorySSAWrapperPass>(*F).getMSSA();
    MSSA.print(dbgs());
    // EDTGraphCache Cache(M, NM, MSSA);
    // EDTGraph EDTG(Cache);
    // EDTG.print();

    // /// Print module info
    // LLVM_DEBUG(
    //     dbgs() << "\n-------------------------------------------------\n");
    // // LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
    // LLVM_DEBUG(dbgs() << TAG << "Process has finished\n");
    // LLVM_DEBUG(
    //     dbgs() << "\n-------------------------------------------------\n");
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<MemorySSAWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
    AU.addRequired<Noelle>();
    ModulePass::getAnalysisUsage(AU);
  }
};

} // namespace

// Next there is code to register your pass to "opt"
char ARTSAnalysisPass::ID = 0;
static RegisterPass<ARTSAnalysisPass> X("ARTSAnalysisPass",
                                        "ARTS Analysis Pass", false, false);