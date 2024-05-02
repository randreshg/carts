#ifndef LLVM_ARTS_UTILS_H
#define LLVM_ARTS_UTILS_H

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "noelle/core/Noelle.hpp"
#include "llvm/Analysis/CallGraph.h"


namespace arts {
using BlockSequence = SmallVector<BasicBlock *, 0>;

/// This function finds the inputs and outputs of a region.
void getInputsOutputs(BlockSequence Region, DominatorTree *DT,
                      SetVector<Value *> &Inputs,
                      SetVector<Value *> &Outputs,
                      SetVector<Value *> &Sinks);

/// It finds the BBs that are dominated by FromBB and add
/// them to the DominatedBlocks vector.
void getDominatedBBs(BasicBlock *FromBB, DominatorTree &DT,
                     BlockSequence &DominatedBlocks);

/// This function finds the calls that are dominated by CB and add
/// them to the Calls vector.
void getDominatedCalls(CallBase *CB, DominatorTree *DT, 
                       SmallSetVector<CallBase *, 16> &Calls);

/// Remove values interface
void removeValue(Value *V, bool RecursiveRemove = false,
                 bool RecursiveUndef = true,  Instruction *Exclude = nullptr);

void removeValues(SmallVector<Value *, 16> ValuesToRemove);

/// Function to replace uses of a Value with UndefValue. 
/// - Instructions can be removed if requested.
/// - The processs can also be performed in a recursive way by replacing
///   uses of the instructions that use the value with UndefValue.
void replaceValueWithUndef(Value *V, bool RemoveInsts = false, 
                           bool Recursive = true, Instruction *Exclude = nullptr);

/// Given a basic block, this function creates a function that contains
/// the instructions of the BB. The function is then inserted to the
/// module.
Function *
createFunction(DominatorTree *DT, BasicBlock *FromBB,
                bool DTAnalysis = false, std::string FunctionName = "",
                SmallVector<Value *, 0> *ExcludeArgsFromAggregate = nullptr);

/// Removes the lifetime markers from the function.
void removeLifetimeMarkers(Function &F);


namespace omp {
/// OMP INFO 
/// Helper Struct to get OpenMP related information
struct OMPInfo {
  enum RTFType {
    OTHER = 0,
    PARALLEL,
    PARALLEL_FOR,
    TASKALLOC,
    TASK,
    TASKWAIT,
    TASKDEP,
    SET_NUM_THREADS
  };

  /// Helper Functions
  static RTFType getRTFunction(Function *F) {
    if (!F)
      return RTFType::OTHER;
    auto CalleeName = F->getName();
    if (CalleeName == "__kmpc_fork_call")
      return RTFType::PARALLEL;
    if (CalleeName == "__kmpc_omp_task_alloc")
      return RTFType::TASKALLOC;
    if (CalleeName == "__kmpc_omp_task")
      return RTFType::TASK;
    if (CalleeName == "__kmpc_omp_task_alloc_with_deps")
      return RTFType::TASKDEP;
    if (CalleeName == "__kmpc_omp_taskwait")
      return RTFType::TASKWAIT;
    if (CalleeName == "omp_set_num_threads")
      return RTFType::SET_NUM_THREADS;
    if (CalleeName == "__kmpc_for_static_init_4")
      return RTFType::PARALLEL_FOR;
    return RTFType::OTHER;
  }

  static RTFType getRTFunction(CallBase &CB) {
    auto *Callee = CB.getCalledFunction();
    return getRTFunction(Callee);
  }

  static RTFType getRTFunction(Instruction *I) {
    auto *CB = dyn_cast<CallBase>(I);
    if (!CB)
      return RTFType::OTHER;
    return getRTFunction(*CB);
  }

  static bool isTaskFunction(Function *F) {
    auto RT = getRTFunction(F);
    if (RT == RTFType::TASK || RT == RTFType::TASKDEP ||
        RT == RTFType::TASKWAIT)
      return true;
    return false;
  }

  static bool isRTFunction(CallBase &CB) {
    auto RT = getRTFunction(CB);
    if (RT != RTFType::OTHER)
      return true;
    return false;
  }
};
}
}

#endif // LLVM_ARTS_UTILS_H