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
    /// Fetch NOELLE Manager
    auto &NM = getAnalysis<Noelle>();
    /// Use NOELLE
    // auto Insts = NM.numberOfProgramInstructions();
    // /// Fetch the PDG
    // auto &PDG = *NM.getProgramDependenceGraph();

    /// Fetch the FDG of "main"
    auto FM = NM.getFunctionsManager();
    auto MainFunction = FM->getEntryFunction();
    // auto FDG = PDG->createFunctionSubgraph(*MainFunction);

    /// Identify number of OpenMP regions
    auto AIB = ARTSIRBuilder(M);
    auto AA = ARTSAnalyzer(M, NM, AIB);

    // AA.getNumberofOpenMPRegions(M);
    AA.identifyEDTs(*MainFunction);

    /// Print module info
    LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
    LLVM_DEBUG(dbgs() << TAG << "Module: " << M);
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<Noelle>();
  }
};

}// namespace

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

/// CARTS optimizations pass.
// class CARTSPass : public PassInfoMixin<CARTSPass> {
// public:
//   PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
//     LLVM_DEBUG(dbgs() << "\n ---------------------------------------- \n");
//     LLVM_DEBUG(dbgs() << TAG << "Running CARTS on Module: \n"
//                       << M.getName() << "\n");

//     // Assuming you did not change anything of the IR code
//     return PreservedAnalyses::all();
//   }
// };

// } // namespace

// // This part is the new way of registering your pass
// extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
// llvmGetPassPluginInfo() {
//   return {LLVM_PLUGIN_API_VERSION, "CARTS", "v0.1", [](PassBuilder &PB) {
//             PB.registerPipelineParsingCallback(
//                 [](StringRef Name, ModulePassManager &MPM,
//                    ArrayRef<PassBuilder::PipelineElement>) {
//                   if (Name == "CARTS") {
//                     MPM.addPass(CARTSPass());
//                     return true;
//                   }
//                   return false;
//                 });
//           }};