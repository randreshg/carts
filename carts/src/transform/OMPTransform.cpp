// Description: Main file for the Compiler for ARTS (OmpTransform) pass.
#include <cassert>
#include <cstdint>

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/IPO/Attributor.h"

#include "carts/transform/OMPTransform.h"
#include "carts/utils/ARTS.h"
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
  /// Handle main function
  Function &MainFn = *M.getFunction("main");
  Function &MainEDTFn = *handleMain(MainFn);
  identifyEDTs(MainEDTFn);
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Module after Identifying EDTs\n" << M << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return true;
}

bool OMPTransform::identifyEDTs(Function &Fn) {
  /// If the function is not analyzable, return
  if (Fn.isDeclaration() && !Fn.hasLocalLinkage())
    return false;

  /// If the function has already been processed, return
  if (!VisitedFunctions.insert(&Fn))
    return false;

  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Processing function: " << Fn.getName() << "\n");
  removeLifetimeMarkers(Fn);

  bool EDTFound = false;
  /// Get entry block
  BasicBlock *CurrentBB = &(Fn.getEntryBlock());
  BasicBlock *NextBB = nullptr;
  do {
    NextBB = CurrentBB->getNextNode();
    /// Get first instruction of the function
    Instruction *CurrentI = &(CurrentBB->front());
    do {
      CallBase *CB = dyn_cast<CallBase>(CurrentI);
      if (!CB)
        continue;
      /// Get the callee
      OMPType RTF = getRTFunction(*CB);
      switch (RTF) {
      case OMPType::PARALLEL: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found:\n" << *CB << "\n");
        CurrentI = handleParallelRegion(*CB);
        EDTFound += true;
      } break;
      case OMPType::TASKALLOC: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Task Region Found:\n" << *CB << "\n");
        CurrentI = handleTaskRegion(*CB);
        EDTFound += true;
      } break;
      case OMPType::TASKWAIT: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Taskwait Found:\n" << *CB << "\n");
        CurrentI = handleTaskWait(*CB);
      } break;
      case OMPType::SINGLE: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Single Region Found:\n" << *CB << "\n");
        CurrentI = handleSingleRegion(*CB);
      } break;
      case OMPType::SINGLE_END: {
        LLVM_DEBUG(dbgs() << TAG << "Single End Region Found:\n"
                          << *CB << "\n");
        Instruction *NextI = CurrentI->getPrevNonDebugInstruction();
        removeValue(CurrentI);
        CurrentI = NextI;
      } break;
      case OMPType::OTHER: {
        LLVM_DEBUG(dbgs() << TAG << "Other Function Found:\n" << *CB << "\n");
        CurrentI = handleOtherFunction(*CB);
      } break;
      default:
        continue;
        break;
      }
    } while ((CurrentI = CurrentI->getNextNonDebugInstruction()));
  } while ((CurrentBB = NextBB));
  LLVM_DEBUG(dbgs() << TAG << "Processing function: " << Fn.getName()
                    << " - Finished\n");
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - -\n");
  return EDTFound;
}

Function *OMPTransform::handleMain(Function &MainFn) {
  LLVM_DEBUG(dbgs() << TAG << "Processing main function\n");
  /// Create void function for the main EDT
  FunctionType *MainEDTFnTy = FunctionType::get(Type::getVoidTy(M.getContext()),
                                                /*isVarArg=*/false);
  Function *MainEDTFn = Function::Create(
      MainEDTFnTy, GlobalValue::InternalLinkage, CARTS_EDT_MAIN, &M);
  replaceTerminatorsWithVoidReturn(&MainFn);
  /// Splice the body of the main function right into the new function
  MainEDTFn->splice(MainEDTFn->begin(), &MainFn);
  /// Insert entry block to MainFn
  BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "entry", &MainFn);
  CallInst::Create(MainEDTFn, {}, "", EntryBB);
  ReturnInst::Create(M.getContext(),
                     ConstantInt::get(Type::getInt32Ty(M.getContext()), 0),
                     EntryBB);
  /// Insert metadata
  EDTIRBuilder IRB(EDTType::Main, MainEDTFn);
  MainEDT::setMetadata(IRB);
  /// Debug it
  LLVM_DEBUG(dbgs() << TAG << "Main EDT Function: " << *MainEDTFn << "\n");
  return MainEDTFn;
}

Instruction *OMPTransform::handleParallelRegion(CallBase &CB) {
  EDTIRBuilder IRB(EDTType::Parallel);
  Function *Fn = getOutlinedFunction(&CB);
  const OMPData &RTI = getRTData(getRTFunction(CB));
  Use *CallArgItr = CB.arg_begin() + RTI.KeepCallArgsFrom;
  Argument *FnArgItr = Fn->arg_begin() + RTI.KeepFnArgsFrom;
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
    Argument *OldFnArgIt = OldFn->arg_begin() + RTI.KeepFnArgsFrom;
    Argument *NewFnArgIt = NewFn->arg_begin();
    for (; OldFnArgIt != OldFn->arg_end(); ++OldFnArgIt, ++NewFnArgIt) {
      Me->insertRewireValue(OldFnArgIt, NewFnArgIt);
    }
  };

  /// Build the EDT
  CallBase *NewCB =
      IRB.buildEDT(&CB, Fn, fillRewiringMapFn, nullptr, CARTS_EDT_PARALLEL);
  ParallelEDT::setMetadata(IRB, -1, -1);
  /// Identify EDTs for the new function
  identifyEDTs(*IRB.getNewFn());
  /// Handle the sync done region
  Instruction *NextInst = handleSyncDoneRegion(*NewCB);
  return NextInst;
}

Instruction *OMPTransform::handleSyncDoneRegion(CallBase &CB) {
  Instruction *SplitInst = CB.getNextNonDebugInstruction();
  /// Split block at Call to EDT
  Function &ParentFn = *CB.getFunction();
  LoopInfo *LI = nullptr;
  /// Get Dominator Tree
  DominatorTree *DT = AG.getAnalysis<DominatorTreeAnalysis>(ParentFn);
  BasicBlock *DoneBB = SplitBlock(CB.getParent(), SplitInst, DT, LI);

  /// Create DoneEDT
  BlockSequence Region = {DoneBB};
  getDominatedBBsFrom(DoneBB, *DT, Region);
  CodeExtractor CE(Region);
  CodeExtractorAnalysisCache CEAC(ParentFn);
  SetVector<Value *> Inputs, Outputs;
  Function *DoneEDTFn = CE.extractCodeRegion(CEAC, Inputs, Outputs);
  assert(DoneEDTFn && "DoneEDTFn is nullptr");
  DoneEDTFn->setName(CARTS_EDT_SYNC_DONE);

  /// Set Metadata
  EDTIRBuilder IRB(EDTType::Task, DoneEDTFn);
  CallBase *DoneCB = dyn_cast<CallBase>(DoneEDTFn->user_back());
  for (auto &Arg : DoneCB->args()) {
    if (dyn_cast<PointerType>(Arg->getType()))
      IRB.insertDep(Arg.get());
    else
      IRB.insertParam(Arg.get());
  }
  TaskEDT::setMetadata(IRB, -1);
  /// Identify EDTs for the new function
  identifyEDTs(*DoneEDTFn);
  return DoneCB;
}

Instruction *OMPTransform::handleTaskRegion(CallBase &CB) {
  /// Analyze return pointer
  /// For the shared variables we are interested in all stores that are done
  /// to the shared fields of the kmp_task_t struct. For the firstprivate
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

  /// Get the size of the kmp_task_t struct. If no struct is found,
  int64_t TaskDataSize = 0;
  const auto *TaskStruct = dyn_cast<StructType>(CB.getType());
  StructType *TaskStructTy =
      TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t");
  if (TaskStructTy) {
    TaskDataSize = static_cast<int64_t>(DL.getTypeAllocSize(
        TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t")));
  }

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
      if (Offset >= TaskDataSize && TaskDataSize != 0) {
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
  Function *Fn = omp::getOutlinedFunction(&CB);
  Argument *TaskDataArg = dyn_cast<Argument>(Fn->arg_begin() + 1);
  /// This assumes the 'disaggregation' happens in the first basic block
  BasicBlock &EntryBB = Fn->getEntryBlock();
  Instruction *TaskDataPtr = &*(EntryBB.begin());
  /// Get offsets and values from the Task Data - Task Outlined function
  DenseMap<int64_t, Value *> OffsetToValueOF;
  CurI = &*EntryBB.begin();
  while ((CurI = CurI->getNextNonDebugInstruction())) {
    if (!isa<LoadInst>(CurI))
      continue;

    LoadInst *L = cast<LoadInst>(CurI);
    Value *Val = L->getPointerOperand();
    // LLVM_DEBUG(dbgs() << TAG << "LoadInst: " << *L << "\n");
    int64_t Offset = -1;
    Value *BasePointer = GetPointerBaseWithConstantOffset(Val, Offset, DL);
    // LLVM_DEBUG(dbgs() << TAG << "BasePointer: " << *BasePointer << "\n");
    // LLVM_DEBUG(dbgs() << TAG << "Offset: " << Offset << "\n");
    if (Offset == -1)
      continue;

    bool Cond = false;
    if (TaskDataSize > 0) {
      Cond = (Offset >= TaskDataSize && BasePointer == TaskDataArg) ||
             (Offset < TaskDataSize && BasePointer == TaskDataPtr);
    } else {
      Cond = (Offset != -1) && (BasePointer == TaskDataPtr);
    }

    if (!Cond)
      continue;

    OffsetToValueOF[Offset] = L;
    if (OffsetToValueOF.size() == ValueToOffsetTD.size())
      break;
  }

  // for(auto &V : OffsetToValueOF) {
  //   LLVM_DEBUG(dbgs() << TAG << "OffsetToValueOF: " << V.first << " -> " <<
  //   *V.second << "\n");
  // }
  // for(auto &V : ValueToOffsetTD) {
  //   LLVM_DEBUG(dbgs() << TAG << "ValueToOffsetTD: " << *V.first << " -> " <<
  //   V.second << "\n");
  // }
  assert(ValueToOffsetTD.size() == OffsetToValueOF.size() &&
         "ValueToOffsetTD and ValueToOffsetOF have different sizes");

  /// Fill the EDTRewireMap
  auto &CallArgs = IRB.getCallArgs();
  auto fillRewiringMapFn = [&](EDTIRBuilder *Me, Function *OldFn,
                               Function *NewFn) {
    for (uint32_t CallArgsItr = 0; CallArgsItr < CallArgs.size();
         ++CallArgsItr) {
      Value *CallArg = CallArgs[CallArgsItr];
      // LLVM_DEBUG(dbgs() << TAG << "CallArg: " << *CallArg << "\n");
      Value *OldValue = OffsetToValueOF[ValueToOffsetTD[CallArg]];
      Value *NewValue = NewFn->arg_begin() + CallArgsItr;
      // LLVM_DEBUG(dbgs() << TAG << "Rewiring: " << *OldValue << " -> " <<
      // *NewValue << "\n");
      Me->insertRewireValue(OldValue, NewValue);
    }
  };
  /// Build the EDT
  auto *NewCB =
      IRB.buildEDT(&CB, Fn, fillRewiringMapFn, CallToOmpTask, CARTS_EDT_TASK);
  TaskEDT::setMetadata(IRB, -1);
  identifyEDTs(*IRB.getNewFn());
  return NewCB;
}

Instruction *OMPTransform::handleTaskWait(CallBase &CB) {
  // LLVM_DEBUG(dbgs() << TAG << "Processing Taskwait\n");
  // LLVM_DEBUG(dbgs() << TAG << "Function before: " << *CB.getFunction() << "\n");
  Instruction *CallToDoneEDT = handleSyncDoneRegion(CB);
  Function *DoneFn = CallToDoneEDT->getFunction();
  removeValue(&CB);

  // LLVM_DEBUG(dbgs() << TAG << "Function after: " << *DoneFn << "\n");
  /// Create EDTSync
  AG.invalidateAnalyses();
  DominatorTree *DT = AG.getAnalysis<DominatorTreeAnalysis>(*DoneFn);
  BlockSequence Region;
  getDominatedBBsTo(CallToDoneEDT->getParent(), *DT, Region);
  // LLVM_DEBUG(dbgs() << TAG << "Region: " << Region.size() << "\n");
  // for (auto &BB : Region) {
  //   LLVM_DEBUG(dbgs() << TAG << "BB: " << BB->getName() << "\n");
  // }

  CodeExtractor CE(Region);
  assert(CE.isEligible() && "Region is not eligible");
  SetVector<Value *> Inputs, Outputs;
  CodeExtractorAnalysisCache CEAC(*DoneFn);
  Function *SyncEDTFn = CE.extractCodeRegion(CEAC, Inputs, Outputs);
  assert(SyncEDTFn && "SyncEDTFn is nullptr");
  SyncEDTFn->setName(CARTS_EDT_SYNC);
  // LLVM_DEBUG(dbgs() << TAG << "SyncEDTFn: " << *SyncEDTFn << "\n");
  CallBase *SyncCB = dyn_cast<CallBase>(SyncEDTFn->user_back());
  // LLVM_DEBUG(dbgs() << TAG << "SyncCB: " << *SyncCB << "\n");
  // LLVM_DEBUG(dbgs() << TAG << "SyncCBParent: " << *SyncCB->getFunction()
  //                   << "\n");
  /// Set Metadata to sync EDTs that sync only childs
  EDTIRBuilder IRB(EDTType::Sync, SyncEDTFn);
  for (auto &Arg : SyncCB->args()) {
    if (dyn_cast<PointerType>(Arg->getType()))
      IRB.insertDep(Arg.get());
    else
      IRB.insertParam(Arg.get());
  }
  SetVector<EDT *> EDTInputs, EDTOutputs;
  SyncEDT::setMetadata(IRB, EDTInputs, EDTOutputs, /*SyncChilds=*/true,
                       /*SyncDescendants=*/false);
  // SyncEDTFn->setName(CARTS_EDT_SYNC);
  return SyncCB;
}

Instruction *OMPTransform::handleSingleRegion(CallBase &CB) {
  /// Check if it safely to convert the parallel EDT to a single EDT
  Function &ParentFn = *CB.getFunction();
  // LLVM_DEBUG(dbgs() << TAG << "Function:\n" << ParentFn << "\n");
  BasicBlock *SingleBB = CB.getParent();
  BranchInst *BranchInstruction = cast<BranchInst>(SingleBB->getTerminator());

  /// Start by checking the False successor. If it is a barrier, and the next
  /// instruction is a return instruction, we can safely remove the BB
  BasicBlock *FalseBB = BranchInstruction->getSuccessor(0);
  Instruction *BarrierInst = &FalseBB->front();
  assert(getRTFunction(BarrierInst) == OMPType::BARRIER &&
         "First instruction of the False BB is not Barrier");
  ReturnInst *RI = dyn_cast<ReturnInst>(FalseBB->getTerminator());
  assert(RI &&
         "FalseBB does not have a return instruction - Expected return void");
  assert(BarrierInst->getNextNonDebugInstruction() == RI &&
         "FalseBB has more than one instruction");
  removeValue(BarrierInst);
  FalseBB->setName("exit");

  /// Then, analyze the True successor. Find the 'kmpc_end_single' call and
  /// remove it
  /// NOTE: This will happen in the identifyEDTs function

  /// Get the next instruction
  BasicBlock *TrueBB = BranchInstruction->getSuccessor(1);
  Instruction *CurI = &TrueBB->front();

  /// Remove the single BB
  SingleBB->eraseFromParent();

  /// TrueBB is now the entry block
  TrueBB->setName("entry");

  /// Convert Parallel EDT to Single EDT
  EDTIRBuilder IRB(EDTType::Sync, &ParentFn);
  CallBase *ParentCall = dyn_cast<CallBase>(ParentFn.user_back());
  for (auto &Arg : ParentCall->args()) {
    if (dyn_cast<PointerType>(Arg->getType()))
      IRB.insertDep(Arg.get());
    else
      IRB.insertParam(Arg.get());
  }
  SetVector<EDT *> EDTInputs, EDTOutputs;
  SyncEDT::setMetadata(IRB, EDTInputs, EDTOutputs, /*SyncChilds=*/true,
                       /*SyncDescendants=*/true);
  return CurI;
}

Instruction *OMPTransform::handleOtherFunction(CallBase &CB) {
  Instruction *CurI = &CB;

  /// Identify EDTs for the current function
  Function &Fn = *CB.getCalledFunction();
  bool FoundEDT = identifyEDTs(Fn);

  /// If no EDTs were found, return
  if (!FoundEDT)
    return CurI;

  /// If we get to this point, it means that the function has EDTs.
  /// Convert the function to an EDT
  EDTIRBuilder IRB(EDTType::Task, &Fn);
  CallBase *ParentCall = dyn_cast<CallBase>(Fn.user_back());
  for (auto &Arg : ParentCall->args()) {
    if (dyn_cast<PointerType>(Arg->getType()))
      IRB.insertDep(Arg.get());
    else
      IRB.insertParam(Arg.get());
  }

  /// Build the EDT
  TaskEDT::setMetadata(IRB, -1);
  Fn.setName(CARTS_EDT);
  return CurI;
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
  if (CalleeName == "__kmpc_single")
    return OMPType::SINGLE;
  if (CalleeName == "__kmpc_end_single")
    return OMPType::SINGLE_END;
  if (CalleeName == "__kmpc_barrier")
    return OMPType::BARRIER;
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