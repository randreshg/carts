#include <sys/types.h>

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Use.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSUtils.h"

using namespace llvm;

/// DEBUG
#define DEBUG_TYPE "arts-utils"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ARTSUtils
namespace arts {
namespace utils {

/// ------------------------------------------------------------------- ///
/// --------------------------- ARTS UTILS ---------------------------- ///
/// ------------------------------------------------------------------- ///
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
      if (RecursiveRemove) {
        for (auto *CallArgItr = CBI->arg_begin(); CallArgItr != CBI->arg_end();
             ++CallArgItr) {
          Value *Arg = *CallArgItr;
          if (!isa<PointerType>(Arg->getType()))
            continue;
          removeValue(Arg, CBI, RecursiveRemove, RecursiveUndef);
        }
      } else {
        for (auto *CallArgItr = CBI->arg_begin(); CallArgItr != CBI->arg_end();
             ++CallArgItr) {
          CallArgItr->set(UndefValue::get((*CallArgItr)->getType()));
        }
      }
      ExcludeInst = nullptr;
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
} // namespace utils
} // namespace arts