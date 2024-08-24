#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Use.h"

// #include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <sys/types.h>

// #include "carts/utils/ARTS.h"
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

SetVector<Value *> ValuesToRemove;
/// ------------------------------------------------------------------- ///
/// --------------------------- ARTS UTILS ---------------------------- ///
/// ------------------------------------------------------------------- ///
void getDominatedBBsFrom(BasicBlock *FromBB, DominatorTree &DT,
                         BlockSequence &DominatedBlocks) {
  Function &Fn = *FromBB->getParent();
  for (BasicBlock &ToBB : Fn) {
    if (&ToBB == FromBB)
      continue;
    if (DT.dominates(FromBB, &ToBB))
      DominatedBlocks.push_back(&ToBB);
  }
}

void getDominatedBBsTo(BasicBlock *ToBB, DominatorTree &DT,
                       BlockSequence &DominatedBlocks) {
  Function &Fn = *ToBB->getParent();
  for (BasicBlock &FromBB : Fn) {
    if (&FromBB == ToBB)
      continue;
    if (DT.dominates(&FromBB, ToBB))
      DominatedBlocks.push_back(&FromBB);
  }
}

void rewireValues(DenseMap<Value *, Value *> &RewiringMap, Function *Parent) {
  LLVM_DEBUG(dbgs() << "Rewiring new function arguments:\n");
  for (auto &MapItr : RewiringMap) {
    Value *OldValue = MapItr.first;
    Value *NewValue = MapItr.second;
    if (!OldValue)
      continue;
    assert(OldValue->getType() == NewValue->getType() && "Types do not match");
    LLVM_DEBUG(dbgs() << "  - Rewiring: " << *OldValue << " -> " << *NewValue
                      << "\n");
    if (Parent) {
      OldValue->replaceUsesWithIf(
          NewValue, [Parent](Use &U) { return U.getUser() != Parent; });
    } else
      OldValue->replaceAllUsesWith(NewValue);
  }
  RewiringMap.clear();
}

void removeFunction(Function *Fn) {
  for (auto &Arg : Fn->args())
    replaceUsesWithUndef(&Arg, false, false);
  /// Iterate over function operators and remove them
  for (auto &BB : *Fn) {
    for (auto &I : BB) {
      removeValue(&I, false, false);
    }
  }
  Fn->deleteBody();
  removeValue(Fn, false, false);
}

void removeValue(Value *V, bool RecursiveRemove, bool RecursiveUndef) {
  removeValue(V, nullptr, RecursiveRemove, RecursiveUndef);
}

void removeValue(Value *V, Instruction *ExcludeInst, bool RecursiveRemove,
                 bool RecursiveUndef, bool UndefineUses) {
  if (!V || isa<UndefValue>(V) || V == ExcludeInst)
    return;

  /// Instructions
  if (auto *I = dyn_cast<Instruction>(V)) {
    if (I->getParent() == nullptr)
      return;
    if (UndefineUses)
      replaceUsesWithUndef(V, ExcludeInst, RecursiveRemove, RecursiveUndef);
    LLVM_DEBUG(dbgs() << TAG << "   - Removing instruction: " << *I << "\n");
    /// If the instruction is a call to an OMP runtime function, we need to
    /// remove the call and its arguments
    I->eraseFromParent();
    return;
  }

  if (UndefineUses)
    replaceUsesWithUndef(V, ExcludeInst, RecursiveRemove, RecursiveUndef);
  /// Value is not a instruction. It can be a constant, global variable, etc.
  /// Global variables are not instructions, but we still need to remove them
  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
    LLVM_DEBUG(dbgs() << TAG << "   - Removing global variable: " << *GV
                      << "\n");
    GV->eraseFromParent();
    return;
  }

  /// Function
  if (Function *Fn = dyn_cast<Function>(V)) {
    LLVM_DEBUG(dbgs() << TAG << "   - Removing function: " << *Fn << "\n");
    Fn->eraseFromParent();
    return;
  }
}

void removeValues(SetVector<Value *> ValsToRemove) {
  for (Value *V : ValsToRemove) {
    LLVM_DEBUG(dbgs() << TAG << "   - Removing: " << *V << "\n");
    removeValue(V, true, true);
  }
}

void removeValues() {
  removeValues(ValuesToRemove);
  ValuesToRemove.clear();
}

void insertValueToRemove(Value *V) { ValuesToRemove.insert(V); }

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

  LLVM_DEBUG(dbgs() << TAG << "  - Worklist size: " << Worklist.size() << "\n");
  /// Replace uses with UndefValue and mark instructions for removal
  V->replaceAllUsesWith(UndefValue::get(V->getType()));

  auto Depth = 0u;
  while (!Worklist.empty()) {
    Instruction *Inst = Worklist.pop_back_val();
    if (!Inst || (ExcludeInst && Inst == ExcludeInst))
      continue;
    LLVM_DEBUG(dbgs() << TAG << "   - Replacing: " << *Inst << "\n");
    /// Add users of this instruction to the worklist for further processing
    if (Recursive && (Depth <= MaxDepth)) {
      Depth++;
      for (auto &Use : Inst->uses()) {
        if (Instruction *UserInst = dyn_cast<Instruction>(Use.getUser()))
          Worklist.push_back(UserInst);
      }
    }

    /// Replace uses of the argument with UndefValue
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
    /// Mark the instruction for removal
    if (RemoveUses)
      removeValue(Inst, ExcludeInst, false, false, false);
  }
}

void removeLifetimeMarkers(Function &Fn) {
  for (auto &BB : Fn) {
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

void removeDeadInstructions(Function &Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Removing dead instructions from: "
                    << Fn.getName() << "\n");
  for (auto &BB : Fn) {
    for (auto &I : BB) {
      /// If the instruction is trivially dead, remove it
      if (isInstructionTriviallyDead(&I)) {
        LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
        ValuesToRemove.insert(&I);
        continue;
      }
      /// However, if the instruction is not trivially dead, check its operands
      /// If any of them is undef, remove the instruction
      for (auto &Op : I.operands()) {
        if (isa<UndefValue>(Op.get())) {
          LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
          ValuesToRemove.insert(&I);
          break;
        }
      }
    }
  }
  LLVM_DEBUG(dbgs() << TAG << "Removing " << ValuesToRemove.size()
                    << " dead instructions\n");
  removeValues();
  LLVM_DEBUG(dbgs() << "\n");
}

void replaceTerminatorsWithVoidReturn(Function *Fn) {
  Module *M = Fn->getParent();
  for (BasicBlock &BB : *Fn)
    replaceTerminatorsWithVoidReturn(&BB, M);
}

void replaceTerminatorsWithVoidReturn(BasicBlock *BB, Module *M) {
  if (!M)
    M = BB->getParent()->getParent();
  if (ReturnInst *RI = dyn_cast<ReturnInst>(BB->getTerminator())) {
    ReturnInst::Create(M->getContext(), nullptr, RI);
    RI->eraseFromParent();
  }
}

} // namespace utils
} // namespace arts