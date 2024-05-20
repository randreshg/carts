// Description: Main file for the Compiler for ARTS (CARTS) pass.

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Debug.h"

#include "noelle/core/Noelle.hpp"

#include "ARTSAnalyzer.h"
#include "ARTSIRBuilder.h"

using namespace llvm;
using namespace arts;

/// DEBUG
#define DEBUG_TYPE "carts"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

namespace {

struct CARTS : public ModulePass {
  static char ID;

  CARTS() : ModulePass(ID) {}

  bool doInitialization(Module &M) override { return false; }

  bool runOnModule(Module &M) override {
    LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
    LLVM_DEBUG(dbgs() << TAG << "Running CARTS on Module: \n"
                      << M.getName() << "\n");
    LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
    /// Fetch NOELLE Manager
    auto &NM = getAnalysis<Noelle>();
    /// Fetch the FDG of "main"
    auto FM = NM.getFunctionsManager();
    auto MainFunction = FM->getEntryFunction();
    // auto FDG = PDG->createFunctionSubgraph(*MainFunction);

    /// Identify number of OpenMP regions
    auto AIB = ARTSIRBuilder(M);
    auto AA = ARTSAnalyzer(M, NM, AIB);
    AA.identifyEDTs(*MainFunction);
    AA.debug();

    /// Print module info
    LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
    LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }
};

} // namespace

// Next there is code to register your pass to "opt"
char CARTS::ID = 0;
static RegisterPass<CARTS> X("CARTS", "Compiler for ARTS");

// Next there is code to register your pass to "clang"
static CARTS *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {
                                          if (!_PassMaker) {
                                            PM.add(_PassMaker = new CARTS());
                                          }
                                        }); // ** for -Ox
static RegisterStandardPasses
    _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new CARTS());
                }
              }); // ** for -O0