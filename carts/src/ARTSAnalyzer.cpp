#include "ARTSAnalyzer.h"
#include "ARTS.h"
#include "ARTSUtils.h"

#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"

/// DEBUG
#define DEBUG_TYPE "arts-analyzer"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Namespaces
using namespace llvm;
using namespace arts;
using namespace arts_utils;
using namespace arts_utils::omp;

void ARTSAnalyzer::debug() {
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << "EDTs: " << EDTs.size() << "\n");
  for (auto *E : EDTs)
    LLVM_DEBUG(dbgs() << "- " << *E << "\n");
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - -\n");
}

bool ARTSAnalyzer::identifyEDTs(Function &F) {
  /// This function identifies the regions that will be transformed to
  /// have and EDT initializer.
  /// If a basic block contains a call to the following functions, it
  /// most likely can be transformed to an EDT:
  /// - __kmpc_fork_call.
  /// - __kmpc_single
  /// - __kmpc_omp_taskwait or __kmpc_omp_taskwait_deps
  /// - function with a pointer return type (not void) that is used
  ///   after the call.

  /// Remove of lifetime markers
  removeLifetimeMarkers(F);

  LLVM_DEBUG(dbgs() << "--------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Identifying EDTs for: " << F.getName() << "\n");

  /// If function is not IPO amendable, we give up
  if (F.isDeclaration() && !F.hasLocalLinkage())
    return false;

  /// Get entry block
  BasicBlock *CurrentBB = &(F.getEntryBlock());
  BasicBlock *NextBB = nullptr;
  do {
    NextBB = CurrentBB->getNextNode();
    /// Get first instruction of the function
    Instruction *CurrentI = &(CurrentBB->front());
    do {
      /// We are only interested in call instructions
      auto *CB = dyn_cast<CallBase>(CurrentI);
      if (!CB)
        continue;
      /// Get the callee
      omp::Type RTF = omp::getRTFunction(*CB);
      switch (RTF) {
      case omp::PARALLEL: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found: " << *CB << "\n");
        CurrentI = handleParallelRegion(CB);
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
      } break;
      case omp::TASKALLOC: {
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
        LLVM_DEBUG(dbgs() << TAG << "Task Region Found: " << *CB << "\n");
        CurrentI = handleTaskRegion(CB);
        LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
      } break;
      case omp::TASKWAIT: {
        assert(false && "Taskwait not implemented yet");
      } break;
      case omp::OTHER: {
        LLVM_DEBUG(dbgs() << TAG << "Other Function Found: " << "\n  " << *CB
                          << "\n");
      } break;
      default:
        continue;
        break;
      }
    } while ((CurrentI = CurrentI->getNextNonDebugInstruction()));
  } while ((CurrentBB = NextBB));
  return true;
}

void ARTSAnalyzer::analyzeDeps() {
  /// This function analyzes de EDT and finds dependencies between them.
  /// Here it is very important to consider the nature of certain regions:
  /// - Parallel regions: They are executed in parallel, and there is a
  ///   barrier at the end of the region. This means that every change
  ///   performed in the region is visible after the region.
  /// - Task regions: They are asynchronous. This is means there is no
  ///   guarantee that the changes performed in the region are visible
  ///   after the region. This is because the task may not be executed
  ///   immediately.
}

Instruction *ARTSAnalyzer::handleParallelRegion(CallBase *CB) {
  LLVM_DEBUG(dbgs() << "\n" << TAG << "Handling parallel region\n");
  auto *OutlinedFn = omp::getOutlinedFunction(CB);
  /// Create ParallelEDT
  EDT &ParallelEDT = *createEDT(EDT::Type::PARALLEL);
  /// Set EDT Data environment
  DenseMap<Value *, Value *> RewiringMap;
  const Data &RTI = omp::getRTData(omp::getRTFunction(*CB));
  Argument *FnArgItr = OutlinedFn->arg_begin() + RTI.KeepArgsFrom;
  Use *CallArgItr = CB->arg_begin() + RTI.KeepCallArgsFrom;
  for (auto ArgNum = RTI.KeepArgsFrom; ArgNum < OutlinedFn->arg_size();
       ++CallArgItr, ++FnArgItr, ++ArgNum) {
    ParallelEDT.insertValueToEnv(*CallArgItr);
    RewiringMap[FnArgItr] = *CallArgItr;
  }
  /// Preprocessing
  rewireValues(RewiringMap);
  cleanFunction(OutlinedFn);
  /// Add function to EDT
  AIB.insertEDTEntry(ParallelEDT);
  ParallelEDT.cloneAndAddBasicBlocks(OutlinedFn);
  AIB.redirectEntryAndExit(ParallelEDT, &(OutlinedFn->getEntryBlock()));
  ParallelEDT.adjustDataAndControlFlowToUseClones();
  /// InsertEDTCall
  AIB.setInsertPoint(CB);
  auto *NewCall = AIB.insertEDTCall(ParallelEDT);
  ParallelEDT.CallInst = NewCall;
  EDTPerCallBase[NewCall] = &ParallelEDT;

  /// Identify EDTs in the Parallel Body
  LLVM_DEBUG(dbgs() << "\nIdentifying EDTs in the Parallel Body\n");
  identifyEDTs(*ParallelEDT.getTaskBody());

  /// Identify EDTs in the Done Region
  auto *NextInst = handleParallelDoneRegion(CB);

  /// Remove instructions
  removeValue(CB, false, true);
  removeValue(OutlinedFn);
  ParallelEDT.removeDeadInstructions();

  LLVM_DEBUG(dbgs() << "- - - - - -- - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << "ParallelEDT CallInst: " << *ParallelEDT.CallInst
                    << "\n\n");
  LLVM_DEBUG(dbgs() << "ParallelEDT: \n" << *ParallelEDT.getTaskBody());
  LLVM_DEBUG(dbgs() << "- - - - - -- - - - - - - - - - - - - - - - - - -\n");
  return NextInst;
}

Instruction *ARTSAnalyzer::handleParallelDoneRegion(CallBase *CB) {
  LLVM_DEBUG(dbgs() << "\n" << TAG << "Handling done region\n");
  auto *SplitInst = CB->getNextNonDebugInstruction();

  /// Split block at Call to EDT
  LoopInfo *LI = nullptr;
  DominatorTree *DT = nullptr;
  BasicBlock *DoneBB = SplitBlock(CB->getParent(), SplitInst, DT, LI);

  /// Create DoneEDT
  auto &NDT = NM.getDominators(CB->getFunction())->DT;
  auto Region = NDT.getDescendants(DoneBB);
  BlockSequence RegionSeq;
  for (auto *BB : Region)
    RegionSeq.push_back(BB);
  EDT &DoneEDT = *createEDT(EDT::Type::DONE);

  /// Set DataEnvironment
  CodeExtractor CE(RegionSeq);
  SetVector<Value *> Inputs, Outputs, Sinks;
  CE.findInputsOutputs(Inputs, Outputs, Sinks);
  for (auto *I : Inputs)
    DoneEDT.insertValueToEnv(I);

  /// DoneEDT Body
  AIB.insertEDTEntry(DoneEDT);
  DoneEDT.cloneAndAddBasicBlocks(RegionSeq);
  AIB.redirectEntryAndExit(DoneEDT, RegionSeq.front());
  DoneEDT.adjustDataAndControlFlowToUseClones();

  /// InsertEDTCall
  auto *NextInst = CB->getNextNonDebugInstruction();
  assert(isa<BranchInst>(NextInst) && "Next instruction is not a BranchInst");
  AIB.setInsertPoint(NextInst);
  auto *NewCall = AIB.insertEDTCall(DoneEDT);
  DoneEDT.CallInst = NewCall;
  EDTPerCallBase[NewCall] = &DoneEDT;

  /// Replace NextInst with a ret instruction
  IRBuilder<> Builder(NextInst);
  Builder.CreateRet(Builder.getInt32(0));
  /// Remove instructions
  removeValue(NextInst);
  for (auto *BB : RegionSeq)
    BB->eraseFromParent();

  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << "DoneEDT CallInst: " << *DoneEDT.CallInst << "\n\n");
  LLVM_DEBUG(dbgs() << "DoneEDT:\n" << *DoneEDT.getTaskBody());
  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - - - -\n");
  return NewCall;
}

Instruction *ARTSAnalyzer::handleTaskRegion(CallBase *CB) {
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
  LLVM_DEBUG(dbgs() << "\n" << TAG << "Handling task region\n");
  /// Create TaskEDT
  EDT &TaskEDT = *createEDT(EDT::Type::TASK);
  /// Get the size of the kmp_task_t struct
  const DataLayout &DL = CB->getModule()->getDataLayout();
  LLVMContext &Ctx = CB->getContext();
  const auto *TaskStruct = dyn_cast<StructType>(CB->getType());
  const auto TaskDataSize = static_cast<int64_t>(
      DL.getTypeAllocSize(TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t")));

  /// Analyze Task Data
  /// This analysis assumes we only have stores to the task struct
  /// Get offsets and values from the Task Data - Call to __kmpc_omp_task_alloc
  DenseMap<Value *, int64_t> ValueToOffsetTD;
  Instruction *CurI = CB;
  do {
    if (auto *SI = dyn_cast<StoreInst>(CurI)) {
      int64_t Offset = -1;
      auto *Val = SI->getValueOperand();
      GetPointerBaseWithConstantOffset(SI->getPointerOperand(), Offset, DL);
      ValueToOffsetTD[Val] = Offset;
      /// Private variables
      if (Offset >= TaskDataSize) {
        TaskEDT.insertValueToEnv(Val, false);
        continue;
      }
      /// Shared variables
      TaskEDT.insertValueToEnv(Val, true);
      continue;
    }
    /// Break if we find a call to a task function
    if (auto *CI = dyn_cast<CallInst>(CurI)) {
      if (omp::isTaskFunction(*CI))
        break;
    }
  } while ((CurI = CurI->getNextNonDebugInstruction()));

  /// Rewrite Task Outlined Function arguments to match the Task Data
  auto *OutlinedFn = omp::getOutlinedFunction(CB);
  Argument *TaskData = dyn_cast<Argument>(OutlinedFn->arg_begin() + 1);
  /// This assumes the 'desaggregation' happens in the first basic block
  BasicBlock &EntryBB = OutlinedFn->getEntryBlock();
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

  /// Preprocessing
  DenseMap<Value *, Value *> RewiringMap;
  for (auto Itr : ValueToOffsetTD) {
    auto *From = Itr.first;
    auto *To = OffsetToValueOF[Itr.second];
    RewiringMap[To] = From;
  }
  rewireValues(RewiringMap);
  cleanFunction(OutlinedFn);

  /// Add function to EDT
  AIB.insertEDTEntry(TaskEDT);
  TaskEDT.cloneAndAddBasicBlocks(OutlinedFn);
  AIB.redirectEntryAndExit(TaskEDT, &(OutlinedFn->getEntryBlock()));
  TaskEDT.adjustDataAndControlFlowToUseClones();
  /// Get call to __kmpc_omp_task
  CurI = CB;
  while ((CurI = CurI->getNextNonDebugInstruction())) {
    auto *TCB = dyn_cast<CallBase>(CurI);
    if (TCB && omp::getRTFunction(*TCB) == omp::TASK)
      break;
  }
  assert(CurI && "Task RT call not found");
  /// Insert Call to TaskEDT
  AIB.setInsertPoint(CurI);
  auto *NewCall = AIB.insertEDTCall(TaskEDT);
  TaskEDT.CallInst = NewCall;
  EDTPerCallBase[NewCall] = &TaskEDT;

  /// Identify EDTs in the Task Body
  LLVM_DEBUG(dbgs() << "\nIdentifying EDTs in the Task Body\n");
  identifyEDTs(*TaskEDT.getTaskBody());

  /// Remove instructions
  removeValue(CB);
  removeValue(CurI, false);
  removeValue(OutlinedFn);
  TaskEDT.removeDeadInstructions();

  LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << "TaskEDT CallInst: " << *TaskEDT.CallInst << "\n\n");
  LLVM_DEBUG(dbgs() << "TaskEDT \n" << *TaskEDT.getTaskBody());
  LLVM_DEBUG(dbgs() << "- - - - - -- - - - - - - - - - - - - - - - -\n");
  return NewCall;
}

uint64_t ARTSAnalyzer::getNumEDTs() { return EDTs.size(); }

EDT *ARTSAnalyzer::createEDT(EDT::Type Ty) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT with signature: "
                    << *AIB.EdtFunction << "\n");
  auto E = new EDT(Ty, AIB.EdtFunction, M);
  AIB.initializeEDT(*E);
  insertEDT(E);
  return E;
}

void ARTSAnalyzer::insertEDT(EDT *E) {
  EDTs.insert(E);
  EDTPerFunction[E->getTaskBody()] = E;
}

void ARTSAnalyzer::removeEDT(EDT *E) {
  LLVM_DEBUG(dbgs() << TAG << "Removing EDT: " << E->getName() << "\n");
  EDTs.remove(E);
  EDTPerFunction.erase(E->getTaskBody());
}

EDT *ARTSAnalyzer::getEDT(Function *F) {
  return EDTPerFunction.count(F) ? EDTPerFunction[F] : nullptr;
}

EDT *ARTSAnalyzer::getEDT(CallBase *CB) {
  return EDTPerCallBase.count(CB) ? EDTPerCallBase[CB] : nullptr;
}