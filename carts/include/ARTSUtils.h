#ifndef LLVM_ARTS_UTILS_H
#define LLVM_ARTS_UTILS_H

#include "noelle/core/Noelle.hpp"
#include "llvm/Analysis/AssumptionCache.h"
// #include "llvm/Analysis/CallGraph.h"
#include "ARTS.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <cstdint>

namespace arts {
using BlockSequence = SmallVector<BasicBlock *, 0>;

/// This function finds the inputs and outputs of a region.
void getInputsOutputs(BlockSequence Region, DominatorTree *DT,
                      SetVector<Value *> &Inputs, SetVector<Value *> &Outputs,
                      SetVector<Value *> &Sinks);

/// It finds the BBs that are dominated by FromBB and add
/// them to the DominatedBlocks vector.
void getDominatedBBs(BasicBlock *FromBB, DominatorTree &DT,
                     BlockSequence &DominatedBlocks);

/// This function finds the calls that are dominated by CB and add
/// them to the Calls vector.
void getDominatedCalls(CallBase *CB, DominatorTree *DT,
                       SmallSetVector<CallBase *, 16> &Calls);

/// Remove Instruction
void removeInstruction(Instruction *I);

/// Remove values interface
void removeValue(Value *V, bool RecursiveRemove = false,
                 bool RecursiveUndef = true, Instruction *Exclude = nullptr);

void removeValues(SmallVector<Value *, 16> ValuesToRemove);

/// Function to replace uses of a Value with UndefValue.
/// - Instructions can be removed if requested.
/// - The processs can also be performed in a recursive way by replacing
///   uses of the instructions that use the value with UndefValue.
void replaceValueWithUndef(Value *V, bool RemoveInsts = false,
                           bool Recursive = true,
                           Instruction *Exclude = nullptr);
// void replaceFunctionArgWithUndef(Function *F);
// void replaceFunctionArgWithUndef(Function *F, uint32_t ArgNum);
// void replaceFunctionArgWithUndef(Argument *Arg);

/// Given a basic block, this function creates a function that contains
/// the instructions of the BB. The function is then inserted to the
/// module.
Function *createFunction(DominatorTree *DT, BasicBlock *FromBB, bool DTAnalysis,
                         std::string FunctionName,
                         SmallVector<Value *, 0> *ExcludeArgsFromAggregate);

/// Removes the lifetime markers from the function.
void removeLifetimeMarkers(Function &F);

namespace omp {
/// OMP INFO
/// Helper Struct to get OpenMP related information
struct Data {
  uint32_t OutlinedFnPos;
  uint32_t KeepArgsFrom;
  uint32_t KeepCallArgsFrom;
};

enum Type {
  OTHER = 0,
  PARALLEL,
  PARALLEL_FOR,
  TASKALLOC,
  TASK,
  TASKWAIT,
  TASKDEP,
  SET_NUM_THREADS
};

/// Rewire the values in the Call with the arguments of the outlined function.
/// and undef the rest of the values.
void preprocessing(CallBase *Call);
/// Undefine the values previously rewired and its uses in the outlined
/// function.
void postprocessing(CallBase *Call);
/// Helper Functions
Data getRTData(Type RTF);
Type getRTFunction(Function *F);
Type getRTFunction(CallBase &CB);
Type getRTFunction(Instruction *I);
bool isTaskFunction(Function *F);
bool isRTFunction(CallBase &CB);

} // namespace omp
} // namespace arts

#endif // LLVM_ARTS_UTILS_H