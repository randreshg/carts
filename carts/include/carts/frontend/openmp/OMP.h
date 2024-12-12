/// Description: AST Visitor to traverse the AST and find OpenMP directives
#ifndef LLVM_API_CARTS_OPENMP_H
#define LLVM_API_CARTS_OPENMP_H

#include "llvm/IR/CallBase.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

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


#endif // LLVM_API_CARTS_OPENMP_H