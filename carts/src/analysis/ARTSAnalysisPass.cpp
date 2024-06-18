// Description: Main file for the ARTS Analysis pass.
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"

#include "carts/analysis/graph/EDTGraph.h"

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
    EDTNoelleCache Cache(M, NM);
    EDTGraph EDTG(Cache);
    // EDTG.print();

    /// Print module info
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
    LLVM_DEBUG(
        dbgs() << "\n-------------------------------------------------\n");
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }
};

} // namespace

// Next there is code to register your pass to "opt"
char ARTSAnalysisPass::ID = 0;
static RegisterPass<ARTSAnalysisPass> X("ARTSAnalysisPass",
                                        "Analysis pass for ARTS");

// Next there is code to register your pass to "clang"
static ARTSAnalysisPass *_PassMaker = NULL;
static RegisterStandardPasses
    _RegPass1(PassManagerBuilder::EP_OptimizerLast,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new ARTSAnalysisPass());
                }
              }); // ** for -Ox
static RegisterStandardPasses
    _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new ARTSAnalysisPass());
                }
              }); // ** for -O0
