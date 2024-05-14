#include "ARTSAnalyzer.h"
#include "ARTS.h"
#include "ARTSUtils.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include <unordered_set>

/// DEBUG
#define DEBUG_TYPE "arts-analyzer"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Namespaces
using namespace llvm;
using namespace arts;
using namespace arts::omp;

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
  ///
  /// The BB that meet the criteria are split into two BBs. Check example:
  /// BB1:
  ///   %0 = alloca i32, align 4
  ///   %1 = call i32 @__kmpc_fork_call(i32 1, i32 (i32, i8*)*
  ///   @__omp_offloading_fib, i8* %0)
  ///   ...
  ///   ret i32 0
  /// It is transformed to:
  /// BB1:
  ///   %0 = alloca i32, align 4
  ///   %1 = call i32 @__kmpc_fork_call(i32 1, i32 (i32, i8*)*
  ///   @__omp_offloading_fib, i8* %0) br label %edt.done.0
  /// edt.done.0:
  ///   ...
  ///   ret i32 0
  ///
  /// For the example above, the BB that contains the call to __kmpc_fork_call
  /// will be then replaced by the EDT initializer (GUID reserve + call to EDT
  /// allocator) and the parallel outlined function will be the EDT. We then
  /// have to analyze whether the par.done BB needs to be transformed to an
  /// EDT. This is done by analyzing the instructions of the BB to obtain its
  /// data environment. If any of the instructions in the BB uses a shared
  /// variable, the BB is transformed to an EDT.
  ///
  /// By the end of this function, we should have a map that contains the BBs
  /// that can be transformed to EDTs. The key of the map is the BB that
  /// contains the call to the function that creates the region (e.g.
  /// __kmpc_fork_call) and the Val is the struct EDTInfo. Aux variables

  /// Remove of lifetime markers
  removeLifetimeMarkers(F);

  /// Aux variables
  // LoopInfo *LI = nullptr;
  // DominatorTree *DT;
  //  = AG.getAnalysis<DominatorTreeAnalysis>(F);

  LLVM_DEBUG(dbgs() << "\n");
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
        LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found: " << "\n  " << *CB
                          << "\n");
        auto *ParallelEDT = handleParallelRegion(CB);
        auto *DoneEDT = handleDoneRegion(ParallelEDT);
        CurrentI = DoneEDT->CallInst;
      } break;
      case omp::TASKALLOC: {
        LLVM_DEBUG(dbgs() << TAG << "Task Region Found: " << "\n  " << *CB
                          << "\n");

        /// Split block at __kmpc_omp_task_alloc
        // auto TaskItrStr = std::to_string(TaskRegion++);
        // auto TaskName = "task.region." + TaskItrStr;
        // BasicBlock *TaskBB;
        // if (CurrentI != &(CurrentBB->front())) {
        //   TaskBB = SplitBlock(CurrentI->getParent(), CurrentI, DT, LI,
        //   nullptr,
        //                       TaskName);
        // } else {
        //   TaskBB = CurrentBB;
        //   TaskBB->setName(TaskName);
        // }
        // /// Find the task call
        // while ((CurrentI = CurrentI->getNextNonDebugInstruction())) {
        //   auto *TCB = dyn_cast<CallBase>(CurrentI);
        //   if (TCB && omp::getRTFunction(*TCB) == omp::TASK)
        //     break;
        // }
        // assert(CurrentI && "Task RT call not found");
        // /// Remove the task call. We dont need it anymore, and this helps in
        // /// memory analysis
        // auto *NextI = CurrentI->getNextNonDebugInstruction();
        // CurrentI->eraseFromParent();
        // CurrentI = NextI;

        // /// Task are asynchronous. So no need to handle "Done region"
        // if (!isa<ReturnInst>(CurrentI)) {
        //   BasicBlock *TaskDone = SplitBlock(TaskBB, CurrentI, DT, LI,
        //   nullptr);
        //   // NextBB = handleDoneRegion(TaskDone, DT, "task.", TaskItrStr);
        //   NextBB = TaskDone;
        // }

        /// Analyze Outlined Region
        // handleTaskOutlinedRegion(CB);
      } break;
      case omp::TASKWAIT: {
        // /// \Note A taskwait requires an event.
        // /// Split block at __kmpc_omp_taskwait
        // BasicBlock *TaskWaitBB =
        //     SplitBlock(CurrentI->getParent(), CurrentI, DT, LI, nullptr,
        //                "taskwait.region." + std::to_string(TaskRegion));
        // /// Split block again at the next instruction
        // CurrentI = CurrentI->getNextNonDebugInstruction();
        // BasicBlock *TaskWaitDone =
        //     SplitBlock(TaskWaitBB, CurrentI, DT, LI, nullptr,
        //                "taskwait.done." + std::to_string(TaskRegion));
        // NextBB = TaskWaitDone;
        // /// Add the taskwait region to the map
        // // AT.insertEDTBlock(TaskWaitBB, RTF, CB);
        // TaskRegion++;
      } break;
      case omp::OTHER: {
        LLVM_DEBUG(dbgs() << TAG << "Other Function Found: " << "\n  " << *CB
                          << "\n");
        // if (!Callee || !A.isFunctionIPOAmendable(*Callee))
        //   continue;
        // /// Get return type of the callee
        // Type *RetTy = Callee->getReturnType();
        // /// If the return type is void, we are not interested
        // if(RetTy->isVoidTy())
        //   continue;
        /// If the return type is a pointer, it is because we probably would
        /// use the returned Val. For this case, we need to create an EDT.
      } break;
      default:
        continue;
        break;
      }
    } while ((CurrentI = CurrentI->getNextNonDebugInstruction()));
  } while ((CurrentBB = NextBB));
  return false;
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

EDT *ARTSAnalyzer::handleParallelRegion(CallBase *CB) {
  LLVM_DEBUG(dbgs() << "\n" << TAG << "Handling parallel region\n");
  /// Get RTF Info
  const Data &RTI = omp::getRTData(omp::PARALLEL);
  auto *OutlinedFn = dyn_cast<Function>(
      CB->getArgOperand(RTI.OutlinedFnPos)->stripPointerCasts());
  /// Create ParallelEDT
  EDT &ParallelEDT = *createEDT(EDT::Type::PARALLEL);
  /// Set EDTEnv with the Call Arguments
  for (auto ArgNum = RTI.KeepCallArgsFrom; ArgNum < CB->data_operands_size();
       ++ArgNum) {
    auto *Arg = CB->getArgOperand(ArgNum);
    ParallelEDT.insertValueToEnv(Arg);
  }
  /// ParallelEDT Body
  omp::preprocessing(CB);
  AIB.insertEDTEntry(ParallelEDT);
  ParallelEDT.cloneAndAddBasicBlocks(OutlinedFn);
  AIB.redirectEntryAndExit(ParallelEDT, &(OutlinedFn->getEntryBlock()));
  ParallelEDT.adjustDataAndControlFlowToUseClones();
  /// InsertEDTCall
  AIB.setInsertPoint(CB);
  ParallelEDT.CallInst = AIB.insertEDTCall(ParallelEDT);
  omp::postprocessing(CB);
  removeValue(CB, true);
  OutlinedFn->eraseFromParent();
  return &ParallelEDT;
}

EDT *ARTSAnalyzer::handleTaskRegion(CallBase *CB) {
  /// Analyze return pointer
  // For the shared variables we are interested in all stores that are done
  // to the shareds field of the kmp_task_t struct. For the firstprivate
  // variables we are interested in all stores that are done to the
  // privates
  // field of the kmp_task_t_with_privates struct.
  //
  // The returned Val is a pointer to the
  // kmp_task_t_with_privates struct.
  // struct kmp_task_t_with_privates {
  //    kmp_task_t task_data;
  //    kmp_privates_t privates;
  // };
  // typedef struct kmp_task {
  //   void *shareds;
  //   kmp_routine_entry_t routine;
  //   kmp_int32 part_id;
  //   kmp_cmplrdata_t data1;
  //   kmp_cmplrdata_t data2;
  // } kmp_task_t;
  //
  // - For shared variables, the access of the shareds field is obtained by
  //   obtaining stores done to offset 0 of the returned Val of the
  //   taskalloc.
  // - For firstprivate variables, the access of the privates field is
  //   obtained by obtaining stores done to offset 8 of the returned Val of the
  //   kmp_task_t_with_privates struct.

  //   const uint32_t TaskOutlinedFunctionPos = 5;
  //   /// Maps a value to an offset in the task data
  //   DenseMap<Value *, int64_t> ValueToOffsetTD;
  //   /// Maps an offset to a value in the Outlined Function
  //   DenseMap<int64_t, Value *> OffsetToValueOF;

  //   /// Get context and module
  //   const DataLayout &DL = CB->getModule()->getDataLayout();
  //   LLVMContext &Ctx = CB->getContext();

  //   /// Get the size of the kmp_task_t struct
  //   const auto *TaskStruct = dyn_cast<StructType>(CB->getType());
  //   const auto TaskDataSize = static_cast<int64_t>(DL.getTypeAllocSize(
  //       TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t")));

  //   /// Analyze Task Data
  //   /// This analysis assumes we only have stores to the task struct
  //   DataEnv TaskInfoDE;
  //   BasicBlock *BB = CB->getParent();
  //   for (Instruction &I : *BB) {
  //     if (&I == CB)
  //       continue;
  //     if (!isa<StoreInst>(&I))
  //       continue;

  //     int64_t Offset = -1;
  //     auto *S = cast<StoreInst>(&I);
  //     GetPointerBaseWithConstantOffset(S->getPointerOperand(), Offset, DL);
  //     auto *Val = S->getValueOperand();
  //     ValueToOffsetTD[Val] = Offset;

  //     /// Private variables
  //     if(Offset >= TaskDataSize) {
  //       TaskInfoDE.Privates.insert(Val);
  //       continue;
  //     }
  //     /// Shared variables
  //     TaskInfoDE.Shareds.insert(Val);
  //   }

  //   /// Analyze Outlined Function
  //   Function *OutlinedFn =
  //     dyn_cast<Function>(CB->getArgOperand(TaskOutlinedFunctionPos));
  //   Argument *TaskData = dyn_cast<Argument>(OutlinedFn->arg_begin() + 1);
  //   /// This assumes the 'desaggregation' happens in the first basic block
  //   BasicBlock *EntryBB = &OutlinedFn->getEntryBlock();
  //   BasicBlock::iterator Itr = EntryBB->begin();
  //   auto *TaskDataPtr = &*Itr;

  //   // Iterate from the second instruction onwards
  //   ++Itr;
  //   for (; Itr != BB->end(); ++Itr) {
  //     Instruction &I = *Itr;
  //     if(!isa<LoadInst>(&I))
  //       continue;

  //     auto *L = cast<LoadInst>(&I);
  //     auto *Val = L->getPointerOperand();
  //     int64_t Offset = -1;
  //     auto *BasePointer = GetPointerBaseWithConstantOffset(Val, Offset, DL);

  //     if(Offset == -1)
  //       continue;

  //     auto Cond = (Offset >= TaskDataSize && BasePointer == TaskData) ||
  //                       (Offset <  TaskDataSize && BasePointer ==
  //                       TaskDataPtr);
  //     if(!Cond)
  //       continue;

  //     OffsetToValueOF[Offset] = L;
  //     if(OffsetToValueOF.size() == ValueToOffsetTD.size())
  //       break;
  //   }

  //   /// Assert size of value to offset map
  //   assert(ValueToOffsetTD.size() == OffsetToValueOF.size() &&
  //           "ValueToOffsetTD and ValueToOffsetOF have different sizes");

  //   /// Generate new function signature using OffsetToValueTD
  //   SmallVector<Type *, 16> NewArgumentTypes;
  //   for(auto Itr : ValueToOffsetTD) {
  //     auto *V = Itr.first;
  //     NewArgumentTypes.push_back(V->getType());
  //   }
  //   FunctionType *NewFnTy =
  //       FunctionType::get(Type::getVoidTy(Ctx), NewArgumentTypes, false);
  //   LLVM_DEBUG(dbgs() << " - Function rewrite '" << OutlinedFn->getName()
  //                     << "' from " << *OutlinedFn->getFunctionType() << " to
  //                     "
  //                     << *NewFnTy << "\n");

  //   /// Create the new function body and insert it into the module.
  //   Function *NewFn = Function::Create(NewFnTy, OutlinedFn->getLinkage(),
  //                                       OutlinedFn->getAddressSpace());
  //   OutlinedFn->getParent()->getFunctionList().insert(
  //     OutlinedFn->getIterator(), NewFn);
  //   NewFn->takeName(OutlinedFn);
  //   NewFn->setName("task.edt");
  //   OutlinedFn->setSubprogram(nullptr);
  //   OutlinedFn->setMetadata("dbg", nullptr);

  //   /// Create Edt for new function
  //   EdtInfo &TaskEdt = *AT.insertEdt(EdtInfo::TASK, NewFn);

  //   /// Move BB to new function
  //   NewFn->splice(NewFn->begin(), OutlinedFn);
  //   /// Rewire the function arguments.
  //   Argument *NewFnArgItr = NewFn->arg_begin();
  //   for (auto TDItr : ValueToOffsetTD) {
  //     Value *V = OffsetToValueOF[TDItr.second];
  //     V->replaceAllUsesWith(NewFnArgItr);
  //     /// Add to Task data environment
  //     DataEnv::Type Type = TaskInfoDE.getType(TDItr.first);
  //     AT.insertArgToDE(&*NewFnArgItr, Type, TaskEdt);
  //     ++NewFnArgItr;
  //   }

  //   /// Generate CallInst to NewFn
  //   SmallVector<Value *, 0> NewCallArgs;
  //   for (auto TDItr : ValueToOffsetTD) {
  //     auto *Arg = TDItr.first;
  //     NewCallArgs.push_back(Arg);
  //     /// Map Value (Shared) with list of EDTs that use it
  //     // if (PointerType *PT = dyn_cast<PointerType>(Arg->getType()))
  //     //   AT.insertValueToEdt(Arg, &TaskEdt);
  //   }

  //   auto *LastInstruction = BB->getTerminator();
  //   auto *NewCI =
  //       CallInst::Create(NewFn, NewCallArgs, std::nullopt, "",
  //       LastInstruction);
  //   AT.insertEdtCall(NewCI);
  //   NewCI->setTailCallKind(cast<CallInst>(CB)->getTailCallKind());
  //   LLVM_DEBUG(dbgs() << " - New CB: " << *NewCI << "\n");
  //   LLVM_DEBUG(dbgs() << TaskEdt.DE << "\n");

  //   /// Remove Argument 0 and 1 from Original Task Outlined Function
  //   replaceValueWithUndef(OutlinedFn->getArg(0), true);
  //   replaceValueWithUndef(OutlinedFn->getArg(1), true);
  //   ValuesToRemove.push_back(CB);

  //   // Iterate through the basic blocks and check/replace terminators
  //   for (auto &NewBB : *NewFn) {
  //     auto *Terminator = NewBB.getTerminator();
  //     if (isa<ReturnInst>(Terminator)) {
  //       IRBuilder<> Builder(Terminator);
  //       Builder.CreateRetVoid();
  //       Terminator->eraseFromParent();
  //     }
  //   }

  //   if(!identifyEDTs(*NewFn))
  //     return false;
  return nullptr;
}

EDT *ARTSAnalyzer::handleDoneRegion(EDT *DomEDT) {
  LLVM_DEBUG(dbgs() << "\n" << TAG << "Handling done region\n");
  auto *CallToEDT = DomEDT->CallInst;
  auto *SplitInst = CallToEDT->getNextNonDebugInstruction();

  //   /// Get first instruction of BB to analyze if we need a new function
  // If Instruction is a return, we do not need a new function...
  // If it is a call to another EDT, we do not need a new function...

  /// Split block at Call to EDT
  LoopInfo *LI = nullptr;
  DominatorTree *DT = nullptr;
  BasicBlock *DoneBB = SplitBlock(CallToEDT->getParent(), SplitInst, DT, LI);
  /// Create DoneEDT
  auto &NDT = NM.getDominators(CallToEDT->getFunction())->DT;
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
  auto *NextInst = CallToEDT->getNextNonDebugInstruction();
  assert(isa<BranchInst>(NextInst) && "Next instruction is not a BranchInst");
  AIB.setInsertPoint(NextInst);
  DoneEDT.CallInst = AIB.insertEDTCall(DoneEDT);
  /// Replace NextInst with a ret instruction
  IRBuilder<> Builder(NextInst);
  Builder.CreateRet(Builder.getInt32(0));
  NextInst->eraseFromParent();
  // Remove BBs
  for (auto *BB : RegionSeq)
    BB->eraseFromParent();
  return &DoneEDT;
}

uint64_t ARTSAnalyzer::getNumEDTs() { return EDTs.size(); }

/// Create EDT
EDT *ARTSAnalyzer::createEDT(EDT::Type Ty) {
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT with signature: "
                    << *AIB.EdtFunction << "\n");
  auto E = new EDT(Ty, AIB.EdtFunction, M);
  AIB.initializeEDT(*E);
  insertEDT(E);
  return E;
}

/// Insert an EDT
void ARTSAnalyzer::insertEDT(EDT *E) {
  EDTs.insert(E);
  // EDTsPerType[E->getType()].insert(E);
}