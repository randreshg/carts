// Description: Main file for the Compiler for ARTS (OmpTransform) pass.
#include "llvm/ADT/SetVector.h"
#include <cstdint>

#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/Attributor.h"

#include "carts/transform/OMPTransform.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSUtils.h"

using namespace llvm;
using namespace arts;
using namespace arts::omp;
using namespace arts::utils;

/// DEBUG
#define DEBUG_TYPE "omp-transform"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                           OMPTransform                              ///
/// ------------------------------------------------------------------- ///
bool OMPTransform::run(ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Running OmpTransform on Module: \n");
  /// Get main function
  auto &MainFn = *M.getFunction("main");
  identifyEDTs(MainFn);
  /// Set metadata for Main EDT
  EDTIRBuilder IRB(EDTType::Main);
  IRB.setMetadata(MainFn);
  
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Module after Identifying EDTs\n" << M << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return true;
}

void OMPTransform::identifyEDTs(Function &Fn) {
  /// If the function is not analyzable, return
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return;
  Functions.insert(&Fn);
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Processing function: " << Fn.getName() << "\n");
  removeLifetimeMarkers(Fn);

  /// Get entry block
  BasicBlock *CurrentBB = &(Fn.getEntryBlock());
  BasicBlock *NextBB = nullptr;
  do {
    NextBB = CurrentBB->getNextNode();
    /// Get first instruction of the function
    Instruction *CurrentI = &(CurrentBB->front());
    do {
      auto *CB = dyn_cast<CallBase>(CurrentI);
      if (!CB)
        continue;
      /// Get the callee
      OMPType RTF = getRTFunction(*CB);
      switch (RTF) {
      case OMPType::PARALLEL: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found:\n" << *CB << "\n");
        CurrentI = handleParallelRegion(*CB);
      } break;
      case OMPType::TASKALLOC: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Task Region Found:\n" << *CB << "\n");
        CurrentI = handleTaskRegion(*CB);
      } break;
      case OMPType::TASKWAIT: {
        assert(false && "Taskwait not implemented yet");
      } break;
      case OMPType::OTHER: {
      } break;
      default:
        continue;
        break;
      }
    } while ((CurrentI = CurrentI->getNextNonDebugInstruction()));
  } while ((CurrentBB = NextBB));
  LLVM_DEBUG(dbgs() << TAG << "Processing function: " << Fn.getName() << " - Finished\n");
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n");
}

Instruction *OMPTransform::handleParallelRegion(CallBase &CB) {
  EDTIRBuilder IRB(EDTType::Parallel);
  auto *Fn = getOutlinedFunction(&CB);
  const OMPData &RTI = getRTData(getRTFunction(CB));
  Use *CallArgItr = CB.arg_begin() + RTI.KeepCallArgsFrom;
  Argument *FnArgItr = Fn->arg_begin() + RTI.KeepFnArgsFrom;
  /// Insert unused arguments
  for (auto *FnArg = Fn->arg_begin(); FnArg != FnArgItr; ++FnArg) {
    IRB.insertUnusedArg(FnArg);
  }
  /// Insert call arguments
  for (; FnArgItr < Fn->arg_end(); ++CallArgItr, ++FnArgItr) {
    Value *CallV = *CallArgItr;
    if (dyn_cast<PointerType>(CallV->getType()))
      IRB.insertDep(CallV);
    else
      IRB.insertParam(CallV);
  }
  /// Fill the EDTRewireMap
  auto fillRewiringMapFn = [&](EDTIRBuilder *Me, Function *OldFn,
                               Function *NewFn) {
    // LLVM_DEBUG(dbgs() << TAG << "RewiringMap for Parallel Region:\n");
    Argument *OldFnArgIt = OldFn->arg_begin() + RTI.KeepFnArgsFrom;
    Argument *NewFnArgIt = NewFn->arg_begin();
    for (; OldFnArgIt != OldFn->arg_end(); ++OldFnArgIt, ++NewFnArgIt) {
      // LLVM_DEBUG(dbgs() << *OldFnArgIt << "->" << *NewFnArgIt << "\n");
      Me->insertMapValue(OldFnArgIt, NewFnArgIt);
    }
  };

  /// Build the EDT
  auto *NewCB = IRB.buildEDT(&CB, Fn, fillRewiringMapFn);
  identifyEDTs(*NewCB->getCalledFunction());
  auto *NextInst = handleParallelDoneRegion(*NewCB);
  return NextInst;
}

Instruction *OMPTransform::handleParallelDoneRegion(CallBase &CB) {
  auto *SplitInst = CB.getNextNonDebugInstruction();
  /// If it is a callbase, check if its a call to a RT function
  if (auto *SCB = dyn_cast<CallBase>(SplitInst)) {
    if (isRTFunction(*SCB))
      return SplitInst;
    /// TODO: What about other callbase?
  }

  /// Split block at Call to EDT
  Function &ParentFn = *CB.getFunction();
  LoopInfo *LI = nullptr;
  DominatorTree *DT = AG.getAnalysis<DominatorTreeAnalysis>(*CB.getFunction());
  BasicBlock *DoneBB = SplitBlock(CB.getParent(), SplitInst, DT, LI);

  /// Create DoneEDT
  BlockSequence Region;
  getDominatedBBs(DoneBB, *DT, Region);

  /// Set DataEnvironment
  CodeExtractor CE(Region);
  CodeExtractorAnalysisCache CEAC(ParentFn);
  SetVector<Value *> Inputs, Outputs;
  auto *DoneEDTFn = CE.extractCodeRegion(CEAC, Inputs, Outputs);
  DoneEDTFn->setName("carts.edt");
  CallBase *DoneCB = dyn_cast<CallBase>(DoneEDTFn->user_back());
  /// Debug DoneCB arguments
  EDTIRBuilder IRB(EDTType::Task);
  for (auto &Arg : DoneCB->args()) {
    if (dyn_cast<PointerType>(Arg->getType()))
      IRB.insertDep(Arg.get());
    else
      IRB.insertParam(Arg.get());
  }
  IRB.setMetadata(*DoneEDTFn);
  return DoneCB;
}

Instruction *OMPTransform::handleTaskRegion(CallBase &CB) {
  /// Analyze return pointer
  /// For the shared variables we are interested in all stores that are done
  /// to the shareds field of the kmp_task_t struct. For the firstprivate
  /// variables we are interested in all stores that are done to the
  /// privates fields of the kmp_task_t_with_privates struct.
  ///
  /// The returned Val is a pointer to the
  /// kmp_task_t_with_privates struct.
  /// struct kmp_task_t_with_privates {
  ///    kmp_task_t task_data;
  ///    kmp_privates_t privates;
  /// };
  /// typedef struct kmp_task {
  ///   void *shareds;
  ///   kmp_routine_entry_t routine;
  ///   kmp_int32 part_id;
  ///   kmp_cmplrdata_t data1;
  ///   kmp_cmplrdata_t data2;
  /// } kmp_task_t;
  ///
  /// - For shared variables, the access of the shareds field is obtained by
  ///   obtaining stores done to offset 0 of the returned Val of the
  ///   taskalloc.
  /// - For firstprivate variables, the access of the privates field is
  ///   obtained by obtaining stores done to offset 8 of the returned Val of the
  ///   kmp_task_t_with_privates struct.
  /// Get the size of the kmp_task_t struct
  EDTIRBuilder IRB(EDTType::Task);
  const DataLayout &DL = CB.getModule()->getDataLayout();
  LLVMContext &Ctx = CB.getContext();
  const auto *TaskStruct = dyn_cast<StructType>(CB.getType());
  const auto TaskDataSize = static_cast<int64_t>(
      DL.getTypeAllocSize(TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t")));

  /// Analyze Task Data
  /// This analysis assumes we only have stores to the task struct
  /// Get offsets and values from the Task Data - Call to __kmpc_omp_task_alloc
  CallBase *CallToOmpTask = nullptr;
  DenseMap<Value *, int64_t> ValueToOffsetTD;
  Instruction *CurI = &CB;
  do {
    if (auto *SI = dyn_cast<StoreInst>(CurI)) {
      int64_t Offset = -1;
      auto *Val = SI->getValueOperand();
      GetPointerBaseWithConstantOffset(SI->getPointerOperand(), Offset, DL);
      ValueToOffsetTD[Val] = Offset;
      /// Private variables
      if (Offset >= TaskDataSize) {
        IRB.insertParam(Val);
        continue;
      }
      /// Shared variables
      IRB.insertDep(Val);
      continue;
    }
    /// Break if we find a call to a task function
    if (auto *CI = dyn_cast<CallInst>(CurI)) {
      if (omp::isTaskFunction(*CI)) {
        CallToOmpTask = CI;
        break;
      }
    }
  } while ((CurI = CurI->getNextNonDebugInstruction()));

  /// Rewrite Task Outlined Function arguments to match the Task Data
  auto *Fn = omp::getOutlinedFunction(&CB);
  Argument *TaskData = dyn_cast<Argument>(Fn->arg_begin() + 1);
  /// This assumes the 'disaggregation' happens in the first basic block
  BasicBlock &EntryBB = Fn->getEntryBlock();
  auto *TaskDataPtr = &*(++EntryBB.begin());
  /// Get offsets and values from the Task Data - Task Outlined function
  DenseMap<int64_t, Value *> OffsetToValueOF;
  CurI = &*EntryBB.begin();
  while ((CurI = CurI->getNextNonDebugInstruction())) {
    if (!isa<LoadInst>(CurI))
      continue;

    auto *L = cast<LoadInst>(CurI);
    auto *Val = L->getPointerOperand();
    int64_t Offset = -1;
    auto *BasePointer = GetPointerBaseWithConstantOffset(Val, Offset, DL);

    if (Offset == -1)
      continue;

    bool Cond = (Offset >= TaskDataSize && BasePointer == TaskData) ||
                (Offset < TaskDataSize && BasePointer == TaskDataPtr);
    if (!Cond)
      continue;

    OffsetToValueOF[Offset] = L;
    if (OffsetToValueOF.size() == ValueToOffsetTD.size())
      break;
  }
  assert(ValueToOffsetTD.size() == OffsetToValueOF.size() &&
         "ValueToOffsetTD and ValueToOffsetOF have different sizes");
  /// Unused arguments
  IRB.insertUnusedArg(Fn->arg_begin());
  IRB.insertUnusedArg(Fn->arg_begin() + 1);

  /// Fill the EDTRewireMap
  auto fillRewiringMapFn = [&](EDTIRBuilder *Me, Function *OldFn,
                               Function *NewFn) {
    for (uint32_t CallArgsItr = 0; CallArgsItr < IRB.CallArgs.size();
         ++CallArgsItr) {
      Value *CallArg = IRB.CallArgs[CallArgsItr];
      // LLVM_DEBUG(dbgs() << TAG << "CallArg: " << *CallArg << "\n");
      Value *OldValue = OffsetToValueOF[ValueToOffsetTD[CallArg]];
      Value *NewValue = NewFn->arg_begin() + CallArgsItr;
      // LLVM_DEBUG(dbgs() << TAG << "Rewiring: " << *OldValue << " -> " << *NewValue << "\n");
      Me->insertMapValue(OldValue, NewValue);
    }
  };
  /// Build the EDT
  auto *NewCB = IRB.buildEDT(&CB, Fn, fillRewiringMapFn, CallToOmpTask);
  identifyEDTs(*NewCB->getCalledFunction());
  return NewCB;
}

/// ------------------------------------------------------------------- ///
/// ------------------------------ OMP -------------------------------- ///
/// ------------------------------------------------------------------- ///
namespace arts::omp {
Function *getOutlinedFunction(CallBase *Call) {
  auto Data = getRTData(getRTFunction(*Call));
  return dyn_cast<Function>(
      Call->getArgOperand(Data.FnPos)->stripPointerCasts());
}

OMPData getRTData(OMPType RTF) {
  switch (RTF) {
  case OMPType::PARALLEL:
    return {2, 2, 3};
  case OMPType::TASKALLOC:
    return {5, 0, 0};
  default:
    return {0, 0, 0};
  }
}

OMPType getRTFunction(Function *Fn) {
  if (!Fn)
    return OMPType::OTHER;
  auto CalleeName = Fn->getName();
  if (CalleeName == "__kmpc_fork_call")
    return OMPType::PARALLEL;
  if (CalleeName == "__kmpc_omp_task_alloc")
    return OMPType::TASKALLOC;
  if (CalleeName == "__kmpc_omp_task")
    return OMPType::TASK;
  if (CalleeName == "__kmpc_omp_task_alloc_with_deps")
    return OMPType::TASKDEP;
  if (CalleeName == "__kmpc_omp_taskwait")
    return OMPType::TASKWAIT;
  if (CalleeName == "omp_set_num_threads")
    return OMPType::SET_NUM_THREADS;
  if (CalleeName == "__kmpc_for_static_init_4")
    return OMPType::PARALLEL_FOR;
  return OMPType::OTHER;
}

OMPType getRTFunction(CallBase &CB) {
  auto *Callee = CB.getCalledFunction();
  return getRTFunction(Callee);
}

OMPType getRTFunction(Instruction *I) {
  auto *CB = dyn_cast<CallBase>(I);
  if (!CB)
    return OMPType::OTHER;
  return getRTFunction(*CB);
}

bool isTaskFunction(Function *Fn) {
  auto RT = getRTFunction(Fn);
  if (RT == OMPType::TASK || RT == OMPType::TASKDEP || RT == OMPType::TASKWAIT)
    return true;
  return false;
}

bool isTaskFunction(CallBase &CB) {
  auto RT = getRTFunction(CB);
  if (RT == OMPType::TASK || RT == OMPType::TASKDEP || RT == OMPType::TASKWAIT)
    return true;
  return false;
}

bool isRTFunction(CallBase &CB) {
  auto RT = getRTFunction(CB);
  if (RT != OMPType::OTHER)
    return true;
  return false;
}
} // namespace arts::omp