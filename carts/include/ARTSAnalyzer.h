#ifndef LLVM_ARTS_ANALYZER_H
#define LLVM_ARTS_ANALYZER_H

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Transforms/IPO/Attributor.h"
#include <cstdint>
#include <sys/types.h>

#include "ARTSIRBuilder.h"

using namespace llvm;
namespace arts {
struct ARTSAnalyzer {
  // ARTSAnalyzer(AnalysisGetter AG) : AG(AG) {}
  ARTSAnalyzer(Module &M, Noelle &NM, ARTSIRBuilder &AIB) : M(M), NM(NM), AIB(AIB) {}

  /// This function identifies the EDTs in the function
  bool identifyEDTs(Function &F);
  /// Analyze EDTs
  void analyzeDeps();
  Instruction *handleParallelRegion(CallBase *CB);
  Instruction *handleParallelDoneRegion(EDT *DomEDT);
  Instruction *handleTaskRegion(CallBase *CB);
  /// EDTs
  uint64_t getNumEDTs();
  EDT *getEDT(Function *F);

private:
  EDT *createEDT(EDT::Type Ty);
  void insertEDT(EDT *E);
  void removeEDT(EDT *E);

  /// Attributes
  Module &M;
  /// Noelle Manager
  Noelle &NM;
  // AnalysisGetter AG;
  /// Program Dependence Graph
  // PDG &DG;
  /// ARTS IR Builder
  ARTSIRBuilder &AIB;
  /// Set of EDTs
  SetVector<EDT *> EDTs;
  /// EDT per function
  DenseMap<Function *, EDT *> EDTPerFunction;
};
} // namespace arts

#endif // LLVM_ARTS_ANALYZER_H