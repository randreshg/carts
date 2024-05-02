#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/RegionInfo.h"

#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"

#include "noelle/core/Noelle.hpp"
#include "ARTS.h"
#include "ARTSAnalyzer.h"

using namespace llvm;
using namespace arcana::noelle;
using namespace arts;
// using BlockSequence = SmallVector<BasicBlock *, 0>;

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

    /// Fetch NOELLE Manager
    auto &NM= getAnalysis<Noelle>();
    /// Use NOELLE
    auto Insts = NM.numberOfProgramInstructions();
    errs() << "The program has " << Insts << " instructions\n";

    /// Fetch the PDG
    auto PDG = NM.getProgramDependenceGraph();

    /// Fetch the FDG of "main"
    auto FM = NM.getFunctionsManager();
    auto MainFunction = FM->getEntryFunction();
    auto FDG = PDG->createFunctionSubgraph(*MainFunction);

    /// Identify number of OpenMP regions
    auto AA = ARTSAnalyzer();
    LLVM_DEBUG(dbgs() << " - OpenMP Regions:" << AA.getNumberofOpenMPRegions(M)
                      << "\n");

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
