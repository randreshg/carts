#ifndef LLVM_ARTS_UTILS_H
#define LLVM_ARTS_UTILS_H

#include <cstdint>

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "ARTS.h"

/// ------------------------------------------------------------------- ///
///                            ARTS UTILS                               ///
/// Set of utilities to handle IR values and functions.
/// ------------------------------------------------------------------- ///
namespace arts_utils {

/// It finds the BBs that are dominated by FromBB and add
/// them to the DominatedBlocks vector.
void getDominatedBBs(BasicBlock *FromBB, DominatorTree &DT,
                     arts::BlockSequence &DominatedBlocks);

/// Rewire the values in the RewiringMap.
///   - Key: Old Value
///   - Value: New Value
void rewireValues(DenseMap<Value *, Value *> &RewiringMap);

/// Function
void cleanFunction(Function *F);
void removeFunction(Function *F);

/// Remove values interface
void removeValue(Value *V, bool RecursiveRemove = false,
                 bool RecursiveUndef = true);
void removeValue(Value *V, Instruction *ExcludeInst,
                 bool RecursiveRemove = false, bool RecursiveUndef = true);
void removeValues(SmallVector<Value *, 16> ValuesToRemove);
/// Function to replace uses of a Value with UndefValue.
/// - Instructions can be removed if requested.
/// - The processs can also be performed in a recursive way by replacing
///   uses of the instructions that use the value with UndefValue.
/// - The depth of the recursion can be controlled.
void replaceUsesWithUndef(Value *V, bool RemoveUses = false,
                          bool Recursive = true,
                          uint32_t MaxDepth = UINT32_MAX);
void replaceUsesWithUndef(Value *V, Instruction *ExcludeInst = nullptr,
                          bool RemoveUses = false, bool Recursive = true,
                          uint32_t MaxDepth = UINT32_MAX);
/// Removes the lifetime markers from the function.
void removeLifetimeMarkers(Function &F);

/// ------------------------------------------------------------------- ///
///                               OMP                                   ///
/// OpenMP related utilities. It contains helper functions to get       ///
/// information about OpenMP regions and runtime function, and          ///
/// check if a function is an OpenMP runtime function.
/// ------------------------------------------------------------------- ///
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

/// Helper Functions
Function *getOutlinedFunction(CallBase *Call);
Data getRTData(Type RTF);
Type getRTFunction(Function *F);
Type getRTFunction(CallBase &CB);
Type getRTFunction(Instruction *I);
bool isTaskFunction(Function *F);
bool isTaskFunction(CallBase &CB);
bool isRTFunction(CallBase &CB);

} // namespace omp
} // namespace arts_utils

#endif // LLVM_ARTS_UTILS_H