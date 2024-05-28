// Description: Main file for the Compiler for ARTS (CARTS) pass.

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Debug.h"

#include "ARTS.h"
#include "noelle/core/Noelle.hpp"

// #include "ARTSAnalyzer.h"
// #include "ARTSIRBuilder.h"
#include "EDTGraph.h"

using namespace llvm;
using namespace arts;
using namespace arcana::noelle;

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
    LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Running CARTS on Module: \n"
                      << M.getName() << "\n");
    LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
    /// Fetch NOELLE Manager
    auto &NM = getAnalysis<Noelle>();
    EDTCache Cache(M, NM);
    EDTGraph EDTG(Cache);
    // EDTG.print();
    /// Fetch the FDG of "main"
    

    /*
     * Fetch the entry point.
     */
    // auto fm = NM.getFunctionsManager();
    // auto mainF = fm->getEntryFunction();

    /*
     * Data flow analyses
     */
    // auto dfa = NM.getDataFlowAnalyses();
    // auto dfr = dfa.runReachableAnalysis(mainF);
    // errs() << "Data flow reachable analysis\n";
    // for (auto &inst : instructions(mainF)) {
    //   errs() << " Next are the instructions reachable from " << inst << "\n";
    //   auto &outSet = dfr->OUT(&inst);
    //   for (auto reachInst : outSet) {
    //     errs() << "   " << *reachInst << "\n";
    //   }
    // }

    /*
     * Fetch the PDG
     */
    // auto PDG = NM.getProgramDependenceGraph();

    /*
     * Fetch the FDG of "main"
     */
    // auto fm = NM.getFunctionsManager();
    // auto mainF = fm->getEntryFunction();
    // auto FDG = PDG->createFunctionSubgraph(*mainF);

    // /*
    //  * Iterate over the dependences
    //  */
    // auto iterF = [](Value *src, DGEdge<Value, Value> *dep) -> bool {
    //   errs() << "   " << *src << " ";

    //   if (isa<ControlDependence<Value, Value>>(dep)) {
    //     errs() << " CONTROL ";
    //   } else {
    //     errs() << " DATA ";
    //     auto dataDep = cast<DataDependence<Value, Value>>(dep);
    //     if (dataDep->isRAWDependence()) {
    //       errs() << " RAW ";
    //     }
    //     if (dataDep->isWARDependence()) {
    //       errs() << " WAR ";
    //     }
    //     if (dataDep->isWAWDependence()) {
    //       errs() << " WAW ";
    //     }
    //     if (isa<MemoryDependence<Value, Value>>(dataDep)) {
    //       errs() << " MEMORY ";
    //       auto memDep = cast<MemoryDependence<Value, Value>>(dataDep);
    //       if (isa<MustMemoryDependence<Value, Value>>(memDep)) {
    //         errs() << " MUST ";
    //       } else {
    //         errs() << " MAY ";
    //       }
    //     }
    //   }

    //   errs() << "\n";
    //   return false;
    // };

    // for (auto &inst : instructions(mainF)) {
    //   errs() << "Instruction \"" << inst << "\" depends on\n";
    //   FDG->iterateOverDependencesTo(&inst, true, true, true, iterF);
    // }

    // for (auto &inst : instructions(mainF)) {
    //   errs() << "Instruction \"" << inst << "\" outgoing dependences\n";
    //   FDG->iterateOverDependencesFrom(&inst, true, true, true, iterF);
    // }

    // for (auto &inst : instructions(mainF)) {
    //   for (auto &inst2 : instructions(mainF)) {
    //     for (auto dep : FDG->getDependences(&inst, &inst2)) {
    //     }
    //   }
    // }


    /// Alias analysis
    // auto AAEngines = NM.getAliasAnalysisEngines();
    // for (auto e : AAEngines) {
    //   LLVM_DEBUG(dbgs() << "   " << e->getName() << "  " << e->getRawPointer() << "\n");
    // }
    // auto FDG = PDG->createFunctionSubgraph(*MainFunction);

    /// Identify number of OpenMP regions
    // auto AIB = ARTSIRBuilder(M);
    // auto AA = ARTSAnalyzer(M, NM, AIB);
    // AA.identifyEDTs(*MainFunction);
    // AA.debug();

    /// Print module info
    LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
    LLVM_DEBUG(dbgs() << TAG << "Process has finished\n\n" << M << "\n");
    LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
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