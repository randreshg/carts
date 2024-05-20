#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Use.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

// #include "noelle/core/Noelle.hpp"
// #include "llvm/Analysis/CallGraph.h"
#include "ARTS.h"
#include "ARTSUtils.h"
#include "llvm/Support/Debug.h"
#include <sys/types.h>

using namespace llvm;
// using namespace arcana::noelle;
// using BlockSequence = SmallVector<BasicBlock *, 0>;

/// DEBUG
#define DEBUG_TYPE "arts-utils"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ARTSUtils
namespace arts {

void getDominatedBBs(BasicBlock *FromBB, DominatorTree &DT,
                     BlockSequence &DominatedBlocks) {
  Function &F = *FromBB->getParent();
  for (auto &ToBB : F) {
    if (DT.dominates(FromBB, &ToBB))
      DominatedBlocks.push_back(&ToBB);
  }
}

void rewireValues(DenseMap<Value *, Value *> &RewiringMap) {
  for (auto &Rewire : RewiringMap) {
    auto *OldValue = Rewire.first;
    auto *NewValue = Rewire.second;
    OldValue->replaceAllUsesWith(NewValue);
  }
}

void cleanFunction(Function *F) {
  for (auto &Arg : F->args())
    replaceUsesWithUndef(&Arg, true, true, 1);
}

void removeFunction(Function *F) {
  for (auto &Arg : F->args())
    replaceUsesWithUndef(&Arg, false, false);
  F->removeFromParent();
}

void removeValue(Value *V, bool RecursiveRemove, bool RecursiveUndef) {
  removeValue(V, nullptr, RecursiveRemove, RecursiveUndef);
}

void removeValue(Value *V, Instruction *ExcludeInst, bool RecursiveRemove,
                 bool RecursiveUndef) {
  if (!V || isa<UndefValue>(V) || V == ExcludeInst)
    return;

  /// Instructions
  if (auto *I = dyn_cast<Instruction>(V)) {
    /// Call instructions
    if (auto *CBI = dyn_cast<CallBase>(I)) {
      /// Iterate through the arguments and replace them with undef using int
      for (uint32_t ArgItr = 0; ArgItr < CBI->data_operands_size(); ArgItr++) {
        Value *Arg = CBI->getArgOperand(ArgItr);
        if (!isa<PointerType>(Arg->getType()))
          continue;

        removeValue(Arg, CBI, RecursiveRemove, RecursiveUndef);
      }
    }
    LLVM_DEBUG(dbgs() << TAG << "   - Removing instruction: " << *I << "\n");
    replaceUsesWithUndef(I, ExcludeInst, RecursiveRemove, RecursiveUndef);
    I->eraseFromParent();
    return;
  }

  /// Value is not a instruction. It can be a constant, global variable, etc.
  replaceUsesWithUndef(V, ExcludeInst, RecursiveRemove, RecursiveUndef);
  /// Global variables are not instructions, but we still need to remove them
  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
    LLVM_DEBUG(dbgs() << TAG << "   - Removing global variable: " << *GV
                      << "\n");
    GV->eraseFromParent();
    return;
  }

  /// Function
  if (Function *F = dyn_cast<Function>(V)) {
    LLVM_DEBUG(dbgs() << TAG << "   - Removing function: " << *F << "\n");
    F->eraseFromParent();
    return;
  }
}

void removeValues(SmallVector<Value *, 16> ValuesToRemove) {
  for (auto *V : ValuesToRemove)
    removeValue(V, true, true);
}

void replaceUsesWithUndef(Value *V, bool RemoveUses, bool Recursive,
                          uint32_t MaxDepth) {
  replaceUsesWithUndef(V, nullptr, RemoveUses, Recursive, MaxDepth);
}

void replaceUsesWithUndef(Value *V, Instruction *ExcludeInst, bool RemoveUses,
                          bool Recursive, uint32_t MaxDepth) {
  /// If the value is undef, we dont need to do anything
  if (!V || isa<UndefValue>(V))
    return;

  LLVM_DEBUG(dbgs() << TAG << "  - Replacing uses of: " << *V << "\n");
  /// Create a worklist to keep track of instructions to be removed
  SmallVector<Instruction *, 16> Worklist;
  /// Initialize the worklist with all uses of the argument
  for (auto &Use : V->uses()) {
    if (Instruction *UserInst = dyn_cast<Instruction>(Use.getUser()))
      Worklist.push_back(UserInst);
  }

  /// Replace uses with UndefValue and mark instructions for removal
  V->replaceAllUsesWith(UndefValue::get(V->getType()));
  auto Depth = 0u;
  while (!Worklist.empty()) {
    Instruction *Inst = Worklist.pop_back_val();
    if (ExcludeInst || Inst == ExcludeInst)
      continue;
    LLVM_DEBUG(dbgs() << TAG << "   - Replacing: " << *Inst << "\n");
    /// Add users of this instruction to the worklist for further processing
    if (Recursive && (Depth >= MaxDepth)) {
      Depth++;
      for (auto &Use : Inst->uses()) {
        if (Instruction *UserInst = dyn_cast<Instruction>(Use.getUser()))
          Worklist.push_back(UserInst);
      }
    }

    /// Replace uses of the argument with UndefValue
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
    /// Mark the instruction for removal
    if (RemoveUses) {
      removeValue(Inst, ExcludeInst, false, false);
      // LLVM_DEBUG(dbgs() << TAG << "    -> Instruction removed\n");
      // Inst->eraseFromParent();
    }
  }
}

void removeLifetimeMarkers(Function &F) {
  for (auto &BB : F) {
    auto InstIt = BB.begin();
    auto InstEnd = BB.end();

    while (InstIt != InstEnd) {
      auto NextIt = InstIt;
      ++NextIt;
      if (auto *IT = dyn_cast<IntrinsicInst>(&*InstIt)) {
        switch (IT->getIntrinsicID()) {
        case Intrinsic::lifetime_start:
        case Intrinsic::lifetime_end:
          IT->eraseFromParent();
          break;
        default:
          break;
        }
      }
      InstIt = NextIt;
    }
  }
}

/// -------------------------------- OMP -------------------------------- ///
namespace omp {
Function *getOutlinedFunction(CallBase *Call) {
  auto Data = getRTData(getRTFunction(*Call));
  return dyn_cast<Function>(
      Call->getArgOperand(Data.OutlinedFnPos)->stripPointerCasts());
}

Data getRTData(omp::Type RTF) {
  switch (RTF) {
  case omp::Type::PARALLEL:
    return {2, 2, 3};
  case omp::Type::TASKALLOC:
    return {5, 0, 0};
  default:
    return {0, 0, 0};
  }
}

omp::Type getRTFunction(Function *F) {
  if (!F)
    return Type::OTHER;
  auto CalleeName = F->getName();
  if (CalleeName == "__kmpc_fork_call")
    return Type::PARALLEL;
  if (CalleeName == "__kmpc_omp_task_alloc")
    return Type::TASKALLOC;
  if (CalleeName == "__kmpc_omp_task")
    return Type::TASK;
  if (CalleeName == "__kmpc_omp_task_alloc_with_deps")
    return Type::TASKDEP;
  if (CalleeName == "__kmpc_omp_taskwait")
    return Type::TASKWAIT;
  if (CalleeName == "omp_set_num_threads")
    return Type::SET_NUM_THREADS;
  if (CalleeName == "__kmpc_for_static_init_4")
    return Type::PARALLEL_FOR;
  return Type::OTHER;
}

omp::Type getRTFunction(CallBase &CB) {
  auto *Callee = CB.getCalledFunction();
  return getRTFunction(Callee);
}

omp::Type getRTFunction(Instruction *I) {
  auto *CB = dyn_cast<CallBase>(I);
  if (!CB)
    return Type::OTHER;
  return getRTFunction(*CB);
}

bool isTaskFunction(Function *F) {
  auto RT = getRTFunction(F);
  if (RT == Type::TASK || RT == Type::TASKDEP || RT == Type::TASKWAIT)
    return true;
  return false;
}

bool isTaskFunction(CallBase &CB) {
  auto RT = getRTFunction(CB);
  if (RT == Type::TASK || RT == Type::TASKDEP || RT == Type::TASKWAIT)
    return true;
  return false;
}

bool isRTFunction(CallBase &CB) {
  auto RT = getRTFunction(CB);
  if (RT != Type::OTHER)
    return true;
  return false;
}
} // namespace omp

} // namespace arts