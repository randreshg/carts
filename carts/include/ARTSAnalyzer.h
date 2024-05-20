//===- ARTSAnalyzer.h ------*- C++ -*-===//
#ifndef LLVM_ARTS_ANALYZER_H
#define LLVM_ARTS_ANALYZER_H

#include <cstdint>
#include <sys/types.h>

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "noelle/core/Noelle.hpp"

#include "ARTSIRBuilder.h"

using namespace llvm;
namespace arts {
struct ARTSAnalyzer {
  ARTSAnalyzer(Module &M, Noelle &NM, ARTSIRBuilder &AIB)
      : M(M), NM(NM), AIB(AIB) {}

  /// Print the EDTs
  void debug();
  /// This function identifies the EDTs in the function
  bool identifyEDTs(Function &F);
  /// Analyze EDTs
  void analyzeDeps();
  Instruction *handleParallelRegion(CallBase *CB);
  Instruction *handleParallelDoneRegion(CallBase *CB);
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
  /// ARTS IR Builder
  ARTSIRBuilder &AIB;
  /// Set of EDTs
  SetVector<EDT *> EDTs;
  /// EDT per function
  DenseMap<Function *, EDT *> EDTPerFunction;
};
} // namespace arts

#endif // LLVM_ARTS_ANALYZER_H