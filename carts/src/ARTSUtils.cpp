#include "llvm/Analysis/AssumptionCache.h"
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
// void getInputsOutputs(BlockSequence Region, DominatorTree *DT,
//                       SetVector<Value *> &Inputs,
//                       SetVector<Value *> &Outputs,
//                       SetVector<Value *> &Sinks) {
//   Function &F = *Region.front()->getParent();
//   AssumptionCache *AC = AG.getAnalysis<AssumptionAnalysis>(F);
//   CodeExtractorAnalysisCache CEAC(F);
//   CodeExtractor CE(Region, DT, /* AggregateArgs */ false, /* BFI */ nullptr,
//                     /* BPI */ nullptr, AC, /* AllowVarArgs */ false,
//                     /* AllowAlloca */ false, /* Suffix */ "");

//   // SetVector<Value *> Inputs, Outputs, Sinks;
//   CE.findInputsOutputs(Inputs, Outputs, Sinks);
//   LLVM_DEBUG(dbgs() << TAG << "  - Inputs: \n");
//   for (auto *I : Inputs) {
//     LLVM_DEBUG(dbgs() << "    -- " <<  *I << "\n");
//   }
//   LLVM_DEBUG(dbgs() << TAG << "  - Outputs: \n");
//   for (auto *O : Outputs) {
//     LLVM_DEBUG(dbgs() << "  -- " << *O << "\n");
//   }
//   LLVM_DEBUG(dbgs() << TAG << "  - Sinks: \n");
//   for (auto *S : Sinks) {
//     LLVM_DEBUG(dbgs() << "    -- " << *S << "\n");
//   }
// }

// void getDominatedBBs(BasicBlock *FromBB, DominatorTree &DT,
//                      BlockSequence &DominatedBlocks) {
//   Function &F = *FromBB->getParent();
//   for (auto &ToBB : F) {
//     if (DT.dominates(FromBB, &ToBB))
//       DominatedBlocks.push_back(&ToBB);
//   }
// }

// void getDominatedCalls(CallBase *CB, DominatorTree *DT,
//                        SmallSetVector<CallBase *, 16> &Calls) {
//   /// Get BB of the call
//   BasicBlock *BB = CB->getParent();
//   BlockSequence DominatedBBs;
//   getDominatedBBs(BB, *DT, DominatedBBs);
//   /// Get calls in dominated BBs
//   for(auto *DominatedBB : DominatedBBs) {
//     for(auto &I : *DominatedBB) {
//       if(auto *CB = dyn_cast<CallBase>(&I)) {
//         Calls.insert(CB);
//       }
//     }
//   }
// }

void removeValue(Value *V, bool RecursiveRemove = false,
                 bool RecursiveUndef = true, Instruction *Exclude = nullptr) {
  if (isa<UndefValue>(V) || V == Exclude)
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

        removeValue(Arg, RecursiveRemove, RecursiveUndef, CBI);
      }
    }
    LLVM_DEBUG(dbgs() << TAG << "   - Removing instruction: " << *I << "\n");
    replaceValueWithUndef(I, RecursiveRemove, RecursiveUndef, Exclude);
    I->eraseFromParent();
    return;
  }

  replaceValueWithUndef(V, RecursiveRemove, RecursiveUndef, Exclude);
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

void replaceValueWithUndef(Value *V, bool RemoveInsts = false,
                           bool Recursive = true,
                           Instruction *Exclude = nullptr) {
  /// If the value is undef, we dont need to do anything
  if (isa<UndefValue>(V))
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
  while (!Worklist.empty()) {
    Instruction *Inst = Worklist.pop_back_val();
    if (Exclude || Inst == Exclude)
      continue;
    LLVM_DEBUG(dbgs() << TAG << "   - Replacing: " << *Inst << "\n");
    /// Add users of this instruction to the worklist for further processing
    if (Recursive) {
      for (auto &Use : Inst->uses()) {
        if (Instruction *UserInst = dyn_cast<Instruction>(Use.getUser()))
          Worklist.push_back(UserInst);
      }
    }

    /// Replace uses of the argument with UndefValue
    Value *Undef = UndefValue::get(Inst->getType());
    Inst->replaceAllUsesWith(Undef);
    /// Mark the instruction for removal
    if (RemoveInsts) {
      LLVM_DEBUG(dbgs() << TAG << "    -> Instruction removed\n");
      Inst->eraseFromParent();
    }
  }
}

// Function *
// createFunction(DominatorTree *DT, BasicBlock *FromBB,
//                bool DTAnalysis = false, std::string FunctionName = "",
//                SmallVector<Value *, 0> *ExcludeArgsFromAggregate = nullptr) {
//   Function &F = *FromBB->getParent();
//   AssumptionCache *AC = AG.getAnalysis<AssumptionAnalysis>(F);
//   CodeExtractorAnalysisCache CEAC(F);

//   /// Collect blocks
//   BlockSequence Region;
//   /// Get all BBs that are dominated by FromBB
//   if (DTAnalysis)
//     getDominatedBBs(FromBB, *DT, Region);
//   else
//     Region.push_back(FromBB);

//   /// Extract code from the region
//   CodeExtractor CE(Region, DT, /* AggregateArgs */ false, /* BFI */ nullptr,
//                     /* BPI */ nullptr, AC, /* AllowVarArgs */ false,
//                     /* AllowAlloca */ true, /* AllocaBlock */ nullptr);

//   assert(CE.isEligible() && "Expected Region outlining to be possible!");

//   if (ExcludeArgsFromAggregate)
//     for (auto *V : *ExcludeArgsFromAggregate)
//       CE.excludeArgFromAggregate(V);

//   /// Generate function
//   Function *OutF = CE.extractCodeRegion(CEAC);
//   if(FunctionName != "")
//     OutF->setName(FunctionName);

//   // LLVM_DEBUG(dbgs() << TAG << TAG << "Function created: " <<
//   OutF->getName() << "\n"); return OutF;
// }

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

} // namespace arts