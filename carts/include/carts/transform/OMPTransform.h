// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#ifndef LLVM_API_CARTS_OMPTRANSFORM_H
#define LLVM_API_CARTS_OMPTRANSFORM_H

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
///                           OMPTransform                              ///
/// Description: Main file for the Compiler for ARTS (OmpTransform) pass.
/// ------------------------------------------------------------------- ///
class OMPTransform {
public:
  /// Interface
  OMPTransform(Module &M, AnalysisGetter &AG) : M(M), AG(AG) {}
  ~OMPTransform() {}
  bool run(ModuleAnalysisManager &AM);
  bool runAttributor(Attributor &A);
  bool identifyEDTs(Function &Fn);

private:
  Function *handleMain(Function &MainFn);
  Instruction *handleParallelRegion(CallBase &CB);
  Instruction *handleSyncDoneRegion(CallBase &CB);
  Instruction *handleTaskRegion(CallBase &CB);
  Instruction *handleTaskWithDeps(CallBase &CB);
  Instruction *handleTaskWait(CallBase &CB);
  Instruction *handleSingleRegion(CallBase &CB);
  Instruction *handleOtherFunction(CallBase &CB);
  ///  Attributes
  Module &M;
  AnalysisGetter &AG;
  SetVector<Function *> VisitedFunctions;
};

/// ------------------------------------------------------------------- ///
///                               OMP                                   ///
/// OpenMP related utilities. It contains helper functions to get       ///
/// information about OpenMP regions and runtime function, and          ///
/// check if a function is an OpenMP runtime function.
/// ------------------------------------------------------------------- ///
namespace omp {
/// OMP INFO
/// Helper Struct to get OpenMP related information
struct OMPData {
  uint32_t FnPos;
  uint32_t KeepFnArgsFrom;
  uint32_t KeepCallArgsFrom;
};

enum OMPType {
  OTHER = 0,
  PARALLEL,
  PARALLEL_FOR,
  SINGLE,
  SINGLE_END,
  TASKALLOC,
  TASK,
  TASKWAIT,
  TASKDEP,
  SET_NUM_THREADS,
  BARRIER,
  GLOBAL_THREAD_NUM
};

/// Helper Functions
Function *getOutlinedFunction(CallBase *Call);
OMPData getRTData(OMPType RTF);
OMPType getRTFunction(Function *F);
OMPType getRTFunction(CallBase &CB);
OMPType getRTFunction(Instruction *I);
// bool isTaskFunction(Function *F);
bool isTaskFunction(CallBase &CB);
bool isTaskWithDepsFunction(CallBase &CB);
bool isRTFunction(CallBase &CB);
} // namespace omp

} // namespace arts
#endif // LLVM_API_CARTS_OMPTRANSFORM_H