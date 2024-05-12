#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AssumptionCache.h"
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

namespace omp {
/// OMP INFO
void preprocessing(CallBase *CB) {
  assert((getRTFunction(CB) == Type::PARALLEL) &&
         "Only parallel region is supported - as of now");
  auto Data = getRTData(Type::PARALLEL);
  auto *OutlinedFn = dyn_cast<Function>(
      CB->getArgOperand(Data.OutlinedFnPos)->stripPointerCasts());

  Argument *FnArgItr = OutlinedFn->arg_begin() + Data.KeepArgsFrom;
  Use *CallArgItr = CB->arg_begin() + Data.KeepCallArgsFrom;
  for (auto ArgNum = Data.KeepArgsFrom; ArgNum < OutlinedFn->arg_size();
       ++CallArgItr, ++FnArgItr, ++ArgNum) {
    FnArgItr->replaceAllUsesWith(*CallArgItr);
  }
  /// Replace with Undef the function arguments that are not needed
  FnArgItr = OutlinedFn->arg_begin();
  for (auto ArgNum = 0u; ArgNum < Data.KeepArgsFrom; ++ArgNum, ++FnArgItr) {
    FnArgItr->replaceAllUsesWith(UndefValue::get(FnArgItr->getType()));
  }
}

void postprocessing(CallBase *CB) {
  assert((getRTFunction(CB) == Type::PARALLEL) &&
         "Only parallel region is supported - as of now");
  auto Data = getRTData(Type::PARALLEL);
  auto *OutlinedFn = dyn_cast<Function>(
      CB->getArgOperand(Data.OutlinedFnPos)->stripPointerCasts());

  Use *CallArgItr = CB->arg_begin() + Data.KeepCallArgsFrom;
  Argument *FnArgItr = OutlinedFn->arg_begin() + Data.KeepArgsFrom;
  for (auto ArgNum = Data.KeepCallArgsFrom; ArgNum < CB->arg_size();
       ++CallArgItr, ++FnArgItr, ++ArgNum) {
    /// Iterate users
    SmallVector<Use *> ListOfUsesToReplace;
    for (User *Usr : (*CallArgItr)->users()) {
      Instruction &UsrI = *cast<Instruction>(Usr);
      if (UsrI.getParent()->getParent() == OutlinedFn) {
        for (Use &U : Usr->operands())
          ListOfUsesToReplace.push_back(&U);
      }
    }
    for (auto *U : ListOfUsesToReplace)
      U->set(UndefValue::get((*CallArgItr)->getType()));
    CallArgItr->set(UndefValue::get((*CallArgItr)->getType()));
  }
}

Data getRTData(omp::Type RTF) {
  switch (RTF) {
  case omp::Type::PARALLEL:
    return {2, 2, 3};
  case omp::Type::TASK:
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

bool isRTFunction(CallBase &CB) {
  auto RT = getRTFunction(CB);
  if (RT != Type::OTHER)
    return true;
  return false;
}
} // namespace omp

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

void removeValue(Value *V, bool RecursiveRemove, bool RecursiveUndef,
                 Instruction *Exclude) {
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

void replaceValueWithUndef(Value *V, bool RemoveInsts, bool Recursive,
                           Instruction *Exclude) {
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

// void replaceFunctionArgWithUndef(Function *F) {
//   for (auto &Arg : F->args())
//     replaceValueWithUndef(&Arg, true, true);
// }

// void replaceFunctionArgWithUndef(Function *F, uint32_t ArgNum) {
//   auto ArgItr = F->arg_begin() + ArgNum;
//   replaceValueWithUndef(&*ArgItr, true, true);
// }

// void replaceFunctionArgWithUndef(Argument *Arg) {
//   replaceValueWithUndef(Arg, true, true);
// }

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