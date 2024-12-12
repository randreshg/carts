/// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#ifndef LLVM_API_CARTS_ARTSTRANSFORM_H
#define LLVM_API_CARTS_ARTSTRANSFORM_H

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/Attributor.h"

namespace arts {
using namespace llvm;

/// ------------------------------------------------------------------- ///
///                           ARTSTransform                              ///
/// Description: Transform the input LLVM IR with ARTS annotations into
/// LLVM IR code with ARTS constructs.
/// ------------------------------------------------------------------- ///
class ARTSTransform {
public:
  /// Interface
  ARTSTransform(Module &M, AnalysisGetter &AG);
  ~ARTSTransform() = default;
  bool run(ModuleAnalysisManager &AM);
  bool runAttributor(Attributor &A);
  bool identifyEDTs(Function &Fn);

private:
  Function *handleMain(Function &MainFn);
  Instruction *handleParallel(CallBase &CB);
  Instruction *handleSyncDoneRegion(CallBase &CB);
  Instruction *handleTask(CallBase &CB);
  // Instruction *handleTaskWithDeps(CallBase &CB);
  // Instruction *handleTaskWait(CallBase &CB);
  Instruction *handleSingle(CallBase &CB);
  Instruction *handleOther(CallBase &CB);
  ///  Attributes
  Module &M;
  AnalysisGetter &AG;
  SetVector<Function *> VisitedFunctions;
};
} // namespace arts

#endif // LLVM_API_CARTS_ARTSTRANSFORM_H