#include "ARTSAnalyzer.h"
#include "llvm/Support/Debug.h"

/// DEBUG
#define DEBUG_TYPE "arts-analyzer"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif
using namespace arts;

bool ARTSAnalyzer::handleParallelOutlinedRegion(CallBase *RTCall) {
  return false;
}

bool ARTSAnalyzer::handleTaskOutlinedRegion(CallBase *CB) {
  return false;

}

bool ARTSAnalyzer::identifyEDTs(Function &F) {
  return false;
}

void ARTSAnalyzer::analyzeDeps() {
}

uint64_t ARTSAnalyzer::getNumberofOpenMPRegions(Module &M) {
  LLVM_DEBUG(dbgs() << TAG << " - OpenMP Regions:" << 10 << "\n");

  return 10;
}

// bool ARTSAnalyzer::handleParallelOutlinedRegion(CallBase *RTCall) {
//   /// Analyze outlined region
//   const uint32_t ParallelOutlinedFunctionPos = 2;
//   const uint32_t KeepArgsFrom = 2;
//   const uint32_t KeepCallArgsFrom = 3;
//   Function *OldFn =
//       dyn_cast<Function>(RTCall->getArgOperand(ParallelOutlinedFunctionPos));
//   /// Remove arguments from the outlined function
//   for (uint32_t ArgItr = 0; ArgItr < KeepArgsFrom; ArgItr++) {
//     Value *Arg = OldFn->args().begin() + ArgItr;
//     removeValue(Arg, true, false);
//   }

//   /// Generate new outlined function
//   SmallVector<Type *, 16> NewArgumentTypes;
//   SmallVector<AttributeSet, 16> NewArgumentAttributes;

//   // Collect replacement argument types and copy over existing attributes.
//   for (auto ArgItr = KeepArgsFrom; ArgItr < OldFn->arg_size(); ArgItr++) {
//     Value *Arg = OldFn->args().begin() + ArgItr;
//     NewArgumentTypes.push_back(Arg->getType());
//   }

//   // Construct the new function type using the new arguments types.
//   FunctionType *OldFnTy = OldFn->getFunctionType();
//   Type *RetTy = OldFnTy->getReturnType();
//   FunctionType *NewFnTy =
//       FunctionType::get(RetTy, NewArgumentTypes, OldFnTy->isVarArg());
//   LLVM_DEBUG(dbgs() << " - Function rewrite '" << OldFn->getName()
//                     << "' from " << *OldFn->getFunctionType() << " to "
//                     << *NewFnTy << "\n");

//   // Create the new function body and insert it into the module.
//   Function *NewFn = Function::Create(NewFnTy, OldFn->getLinkage(),
//                                       OldFn->getAddressSpace(), "");
//   OldFn->getParent()->getFunctionList().insert(OldFn->getIterator(), NewFn);
//   // NewFn->takeName(OldFn);
//   NewFn->setName("parallel.edt");
//   OldFn->setSubprogram(nullptr);

//   // Create Parallel EDT
//   // EdtInfo &ParallelEDT = *AT.insertEdt(EdtInfo::PARALLEL, NewFn);

//   // Since we have now created the new function, splice the body of the old
//   // function right into the new function, leaving the old rotting hulk of the
//   // function empty.
//   NewFn->splice(NewFn->begin(), OldFn);

//   // Collect the new argument operands for the replacement call site.
//   CallBase *OldCB = dyn_cast<CallBase>(RTCall);
//   const AttributeList &OldCallAttributeList = OldCB->getAttributes();
//   SmallVector<Value *, 16> NewArgOperands;
//   SmallVector<AttributeSet, 16> NewArgOperandAttributes;
  
//   for (unsigned OldArgNum = KeepCallArgsFrom;
//         OldArgNum < OldCB->data_operands_size(); ++OldArgNum) {
//     auto *Arg = OldCB->getArgOperand(OldArgNum);
//     NewArgOperands.push_back(Arg);
//     NewArgOperandAttributes.push_back(
//         OldCallAttributeList.getParamAttrs(OldArgNum));
//     /// Map Value (Shared) with list of EDTs that use it
//     // if (PointerType *PT = dyn_cast<PointerType>(Arg->getType()))
//     //   AT.insertValueToEdt(Arg, &ParallelEDT);
//   }

//   // Create a new call or invoke instruction to replace the old one.
//   auto *NewCI =
//       CallInst::Create(NewFn, NewArgOperands, std::nullopt, "", OldCB);
//   NewCI->setTailCallKind(cast<CallInst>(OldCB)->getTailCallKind());
//   // AT.insertEdtforBlock(&ParallelEDT, NewCI->getParent());

//   // Copy over various properties and the new attributes.
//   LLVMContext &Ctx = OldFn->getContext();
//   CallBase *NewCB = NewCI;
//   // AT.insertEdtCall(NewCB);
//   NewCB->setCallingConv(OldCB->getCallingConv());
//   NewCB->takeName(OldCB);
//   NewCB->setAttributes(AttributeList::get(
//       Ctx, OldCallAttributeList.getFnAttrs(),
//       OldCallAttributeList.getRetAttrs(), NewArgOperandAttributes));

//   // Rewire the function arguments.
//   Argument *OldFnArgIt = OldFn->arg_begin() + KeepArgsFrom;
//   Argument *NewFnArgIt = NewFn->arg_begin();
//   for (unsigned OldArgNum = KeepArgsFrom; OldArgNum < OldFn->arg_size();
//         ++OldArgNum, ++OldFnArgIt) {
//     NewFnArgIt->takeName(&*OldFnArgIt);
//     OldFnArgIt->replaceAllUsesWith(&*NewFnArgIt);
//     /// Add to Parallel EDT Data Environment
//     Argument *Arg = &*NewFnArgIt;
//     Type *ArgType = NewFnArgIt->getType();
//     /// For now assume that if it is a pointer, it is a shared variable
//     // if (PointerType *PT = dyn_cast<PointerType>(ArgType))
//     //   AT.insertArgToDE(Arg, DataEnv::SHARED, ParallelEDT);
//     // /// If not, it is a first private variable
//     // else
//     //   AT.insertArgToDE(Arg, DataEnv::FIRSTPRIVATE, ParallelEDT);

//     /// NOTES: The outline function or the variable should have attributes
//     /// that provide more information about the lifetime of the variable. It
//     /// is also important to consider the underlying type of the variable.
//     /// There may be cases, where there is a firstprivate variable that is a
//     /// pointer. For this case, the pointer is private, but the data it points
//     /// to is shared.
//     ++NewFnArgIt;
//   }

//   // Eliminate the instructions *after* we visited all of them.
//   OldCB->replaceAllUsesWith(NewCB);
//   OldCB->eraseFromParent();
//   ValuesToRemove.push_back(OldFn);

//   assert(NewFn->isDeclaration() == false && "New function is a declaration");
//   LLVM_DEBUG(dbgs() << " - New CB: " << *NewCB << "\n");
//   // LLVM_DEBUG(dbgs() << ParallelEDT.DE << "\n");

//   /// Identify EDT for new function
//   if(!identifyEDTs(*NewFn))
//     return false;
//   return true;
// }

// bool ARTSAnalyzer::handleTaskOutlinedRegion(CallBase *CB) {
//   /// Analyze return pointer
//   // For the shared variables we are interested in all stores that are done
//   // to the shareds field of the kmp_task_t struct. For the firstprivate
//   // variables we are interested in all stores that are done to the privates
//   // field of the kmp_task_t_with_privates struct.
//   //
//   // The returned Val is a pointer to the
//   // kmp_task_t_with_privates struct.
//   // struct kmp_task_t_with_privates {
//   //    kmp_task_t task_data;
//   //    kmp_privates_t privates;
//   // };
//   // typedef struct kmp_task {
//   //   void *shareds;
//   //   kmp_routine_entry_t routine;
//   //   kmp_int32 part_id;
//   //   kmp_cmplrdata_t data1;
//   //   kmp_cmplrdata_t data2;
//   // } kmp_task_t;
//   //
//   // - For shared variables, the access of the shareds field is obtained by
//   // obtaining stores done to offset 0 of the returned Val of the taskalloc.
//   // -For firstprivate variables, the access of the privates field is obtained
//   // by obtaining stores done to offset 8 of the returned Val of the
//   // kmp_task_t_with_privates struct.
//   //
//   // The function returns ChangeStatus::CHANGED if the data environment is
//   // updated, ChangeStatus::UNCHANGED otherwise.

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
//                       (Offset <  TaskDataSize && BasePointer == TaskDataPtr);
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
//                     << "' from " << *OutlinedFn->getFunctionType() << " to "
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
//       CallInst::Create(NewFn, NewCallArgs, std::nullopt, "", LastInstruction);
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
//   return true;
// }

// /// Handles the done region and return next BB to analyze
// BasicBlock *ARTSAnalyzer::handleDoneRegion(
//         BasicBlock *DoneBB, DominatorTree *DT, 
//         std::string PrefixName, std::string SuffixBB) {
//   LLVM_DEBUG(dbgs() << "\n" <<  TAG << "Handling done region\n");
//   /// Get first instruction of BB to analyze if we need a new function
//   Instruction *FirstI = &*DoneBB->begin();
//   /// If it is a callbase, check if its a call to a RT function
//   if(auto *CB = dyn_cast<CallBase>(FirstI)) {
//     if(OMPInfo::isRTFunction(*CB))
//       return DoneBB;
//     /// TODO: What about other callbase?
//   }

//   /// Handle other instructions
//   auto DoneFnName = PrefixName + "edt.done";
//   Function *DoneFunction = createFunction(DT, DoneBB, true, DoneFnName);
//   // EdtInfo *DoneEDT = AT.insertEdt(EdtInfo::OTHER, DoneFunction);

//   /// Analyze Data Environment
//   /// The function arguments provide this information.
//   for (auto &Arg : DoneFunction->args()) {
//     Type *ArgType = Arg.getType();
//     /// For now assume that if it is a pointer, it is a shared variable
//     // if (PointerType *PT = dyn_cast<PointerType>(ArgType))
//     //   AT.insertArgToDE(&Arg, DataEnv::SHARED, *DoneEDT);
//     // /// If not, it is a first private variable
//     // else
//     //   AT.insertArgToDE(&Arg, DataEnv::FIRSTPRIVATE, *DoneEDT);
//   }
//   // LLVM_DEBUG(dbgs() << DoneEDT->DE << "\n");

//   /// Get caller of the Done function, and rename BB to par.done
//   auto DoneBBName = PrefixName + "done." + SuffixBB;
//   CallBase *DoneCB = dyn_cast<CallBase>(DoneFunction->user_back());
//   // AT.insertEdtCall(DoneCB);
//   auto *NewDoneBB = DoneCB->getParent();
//   NewDoneBB->setName(DoneBBName);

//   /// Analyze the instructions of the Done function
//   identifyEDTs(*DoneFunction);
//   return NewDoneBB->getNextNode();
// }

// bool ARTSAnalyzer::identifyEDTs(Function &F) {
//   /// This function identifies the regions that will be transformed to
//   /// have and EDT initializer.
//   /// If a basic block contains a call to the following functions, it
//   /// most likely can be transformed to an EDT:
//   /// - __kmpc_fork_call.
//   /// - __kmpc_single
//   /// - __kmpc_omp_taskwait or __kmpc_omp_taskwait_deps
//   /// - function with a pointer return type (not void) that is used
//   ///   after the call.
//   ///
//   /// The BB that meet the criteria are split into two BBs. Check example:
//   /// BB1:
//   ///   %0 = alloca i32, align 4
//   ///   %1 = call i32 @__kmpc_fork_call(i32 1, i32 (i32, i8*)*
//   ///   @__omp_offloading_fib, i8* %0)
//   ///   ...
//   ///   ret i32 0
//   /// It is transformed to:
//   /// BB1:
//   ///   %0 = alloca i32, align 4
//   ///   %1 = call i32 @__kmpc_fork_call(i32 1, i32 (i32, i8*)*
//   ///   @__omp_offloading_fib, i8* %0) br label %edt.done.0
//   /// edt.done.0:
//   ///   ...
//   ///   ret i32 0
//   ///
//   /// For the example above, the BB that contains the call to __kmpc_fork_call
//   /// will be then replaced by the EDT initializer (GUID reserve + call to EDT
//   /// allocator) and the parallel outlined function will be the EDT. We then
//   /// have to analyze whether the par.done BB needs to be transformed to an
//   /// EDT. This is done by analyzing the instructions of the BB to obtain its
//   /// data environment. If any of the instructions in the BB uses a shared
//   /// variable, the BB is transformed to an EDT.
//   ///
//   /// By the end of this function, we should have a map that contains the BBs
//   /// that can be transformed to EDTs. The key of the map is the BB that
//   /// contains the call to the function that creates the region (e.g.
//   /// __kmpc_fork_call) and the Val is the struct EDTInfo. Aux variables

//   /// Remove of lifetime markers
//   removeLifetimeMarkers(F);

//   /// Aux variables
//   LoopInfo *LI = nullptr;
//   DominatorTree *DT = AG.getAnalysis<DominatorTreeAnalysis>(F);

//   /// Region counter
//   uint32_t ParallelRegion = 0;
//   uint32_t TaskRegion = 0;
//   LLVM_DEBUG(dbgs() << "\n");
//   LLVM_DEBUG(dbgs() << TAG << "[identifyEDTs] Identifying EDTs for: " << F.getName() << "\n");

//   /// If function is not IPO amendable, we give up
//   if (F.isDeclaration() && !F.hasLocalLinkage())
//     return false;

//   /// Get entry block
//   BasicBlock *CurrentBB = &(F.getEntryBlock());
//   BasicBlock *NextBB = nullptr;
//   do {
//     NextBB = CurrentBB->getNextNode();
//     /// Get first instruction of the function
//     Instruction *CurrentI = &(CurrentBB->front());
//     do {
//       /// We are only interested in call instructions
//       auto *CB = dyn_cast<CallBase>(CurrentI);
//       if (!CB)
//         continue;
//       /// Get the callee
//       OMPInfo::RTFType RTF = OMPInfo::getRTFunction(*CB);
//       switch (RTF) {
//       case OMPInfo::PARALLEL: {
//         LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found: " << "\n  "
//                           <<  *CB << "\n");

//         /// Split block at __kmpc_parallel
//         auto ParallelItrStr = std::to_string(ParallelRegion++);
//         auto ParallelName = "par.region." + ParallelItrStr;
//         BasicBlock *ParallelBB;
//         if(CurrentI != &(CurrentBB->front())) {
//           ParallelBB = SplitBlock(
//             CurrentI->getParent(), CurrentI, DT, LI, nullptr, ParallelName);
//         }
//         else {
//           ParallelBB = CurrentBB;
//           ParallelBB->setName(ParallelName);
//         }

//         /// Split block at the next instruction
//         CurrentI = CurrentI->getNextNonDebugInstruction();

//         /// Analyze Done Region
//         if(!isa<ReturnInst>(CurrentI)) {
//           BasicBlock *ParallelDone =
//             SplitBlock(ParallelBB, CurrentI, DT, LI, nullptr);
//           NextBB = handleDoneRegion(ParallelDone, DT, "par.", ParallelItrStr);
//         }

//         /// Analyze Outlined Region
//         handleParallelOutlinedRegion(CB);
//       } break;
//       case OMPInfo::TASKALLOC: {
//         LLVM_DEBUG(dbgs() << TAG << "Task Region Found: " << "\n  "
//                           <<  *CB << "\n");

//         /// Split block at __kmpc_omp_task_alloc
//         auto TaskItrStr = std::to_string(TaskRegion++);
//         auto TaskName = "task.region." + TaskItrStr;
//         BasicBlock *TaskBB;
//         if(CurrentI != &(CurrentBB->front())) {
//           TaskBB = SplitBlock(
//             CurrentI->getParent(), CurrentI, DT, LI, nullptr, TaskName);
//         }
//         else {
//           TaskBB = CurrentBB;
//           TaskBB->setName(TaskName);
//         }
//         /// Find the task call 
//         while ((CurrentI = CurrentI->getNextNonDebugInstruction())) {
//           auto *TCB = dyn_cast<CallBase>(CurrentI);
//           if (TCB && OMPInfo::getRTFunction(*TCB) == OMPInfo::TASK)
//             break;
//         }
//         assert(CurrentI && "Task RT call not found");
//         /// Remove the task call. We dont need it anymore, and this helps in memory analysis
//         auto *NextI = CurrentI->getNextNonDebugInstruction();
//         CurrentI->eraseFromParent();
//         CurrentI = NextI;

//         /// Task are asynchronous. So no need to handle "Done region"
//         if(!isa<ReturnInst>(CurrentI)) {
//           BasicBlock *TaskDone =
//             SplitBlock(TaskBB, CurrentI, DT, LI, nullptr);
//           // NextBB = handleDoneRegion(TaskDone, DT, "task.", TaskItrStr);
//           NextBB = TaskDone;
//         }

//         /// Analyze Outlined Region
//         handleTaskOutlinedRegion(CB);
//       } break;
//       case OMPInfo::TASKWAIT: {
//         /// \Note A taskwait requires an event.
//         /// Split block at __kmpc_omp_taskwait
//         BasicBlock *TaskWaitBB =
//             SplitBlock(CurrentI->getParent(), CurrentI, DT, LI, nullptr,
//                         "taskwait.region." + std::to_string(TaskRegion));
//         /// Split block again at the next instruction
//         CurrentI = CurrentI->getNextNonDebugInstruction();
//         BasicBlock *TaskWaitDone =
//             SplitBlock(TaskWaitBB, CurrentI, DT, LI, nullptr,
//                         "taskwait.done." + std::to_string(TaskRegion));
//         NextBB = TaskWaitDone;
//         /// Add the taskwait region to the map
//         // AT.insertEDTBlock(TaskWaitBB, RTF, CB);
//         TaskRegion++;
//       } break;
//       case OMPInfo::OTHER: {
//         // if (!Callee || !A.isFunctionIPOAmendable(*Callee))
//         //   continue;
//         // /// Get return type of the callee
//         // Type *RetTy = Callee->getReturnType();
//         // /// If the return type is void, we are not interested
//         // if(RetTy->isVoidTy())
//         //   continue;
//         /// If the return type is a pointer, it is because we probably would
//         /// use the returned Val. For this case, we need to create an EDT.
//       } break;
//       default:
//         continue;
//         break;
//       }
//     } while ((CurrentI = CurrentI->getNextNonDebugInstruction()));
//   } while ((CurrentBB = NextBB));
//   return false;
// }

// /// Analyze EDTs
// void ARTSAnalyzer::analyzeDeps() {
//   /// This function analyzes de EDT and finds dependencies between them.
//   /// Here it is very important to consider the nature of certain regions:
//   /// - Parallel regions: They are executed in parallel, and there is a
//   ///   barrier at the end of the region. This means that every change 
//   ///   performed in the region is visible after the region.
//   /// - Task regions: They are asynchronous. This is means there is no
//   ///   guarantee that the changes performed in the region are visible
//   ///   after the region. This is because the task may not be executed
//   ///   immediately.

//   /// Analyze callgraph
//   LLVM_DEBUG(dbgs() << TAG << "Analyzing EDTs\n");
//   /// Iterate through all the functions that represents an EDT
  
//   CallGraph &CG = AM.getResult<CallGraphAnalysis>(AT.M);
//   LoopInfo *LI = nullptr;
//   // // CG.dump();

//   // LLVM_DEBUG(dbgs() << "\n---------------------------------\n");
//   for(auto &CGItr : CG) {
//     const Function *F = CGItr.first;
//     /// Skip external functions
//     if (!F || !F->isDefinitionExact())
//       continue;

//     /// Get the Caller EDT -> EDT representing F that calls other EDTs
//     EdtInfo *CallerEdt = AT.getEdt(const_cast<Function*>(F));
//     if(!CallerEdt)
//       continue;
//     LLVM_DEBUG(dbgs() << TAG << "EDT: " << F->getName() << "\n");

//     /// Analyze Callees
//     CallGraphNode *CGN = CGItr.second.get();
//     for(CallGraphNode::CallRecord &Edge : *CGN) {
//       if (!Edge.first)
//         continue;

//       /// We are only concerned with calls to EDTs
//       CallBase *CB = cast<CallBase>(*Edge.first);
//       Function *CalleeFn = CB->getCalledFunction();
//       EdtInfo *CalleeEdt = AT.getEdt(CalleeFn);
//       if(!CalleeEdt)
//         continue;
//       LLVM_DEBUG(dbgs() << "  - Callee: " << CalleeFn->getName() << "\n");

//       /// For now, we are only concerned with Parallel EDTs
//       // if(CalleeEdt->getType() != EdtInfo::PARALLEL)
//       //   continue;

//       /// If CB is in a loop, continue
//       // Loop *L = LI->getLoopFor(CB->getParent());

//       /// If Callee EDT is a parallel region, we are only concerned with the successor
//       /// EDT. (Note: There should be only one successor)

//       /// Get the successor BasicBlock
//       BasicBlock *CBParent = CB->getParent();

//       /// Assert that there is only one successor
      


//       switch(CalleeEdt->getType()){
//         case EdtInfo::PARALLEL: {
//           LLVM_DEBUG(dbgs() << "   - This is a ParallelEDT. Analyzing ParalllelEDT Done \n");
//           /// Get the successor EDT
//           BasicBlock *SuccBB = CBParent->getSingleSuccessor();
//           assert(SuccBB && "There should be only one successor");
//           CallBase *EdtCall = AT.getEdtCall(SuccBB);
//           EdtInfo *SuccEdt = AT.getEdt(EdtCall);
//           assert(SuccEdt && "Successor EDT not found");
//           /// Analyze call arguments t
//           /// Analyze shared variables of CalleeEdt
//           // for(auto &Shared : CalleeEdt->DE.Shareds) {
//           //   /// If the shared variable is not in the successor EDT, add it
//           //   if(!SuccEdt->DE.Shareds.count(Shared)) {
//           //     SuccEdt->DE.Shareds.insert(Shared);
//           //     LLVM_DEBUG(dbgs() << "   - Adding shared variable: " << *Shared << "\n");
//           //   }
//           // }
          
          
//         } break;
//         case EdtInfo::TASK: {
//           // /// Get the successor EDT
//           // EdtInfo *SuccEdt = CalleeEdt->Successors[0];
//           // LLVM_DEBUG(dbgs() << "    - Succ: " << SuccEdt->F->getName() << "\n");
//           // /// Add dependency
//           // CallerEdt->Successors.push_back(SuccEdt);
//           // SuccEdt->Predecessors.push_back(CallerEdt);
//         } break;
//         case EdtInfo::OTHER: {
//           // /// Get the successor EDT
//           // EdtInfo *SuccEdt = CalleeEdt->Successors[0];
//           // LLVM_DEBUG(dbgs() << "    - Succ: " << SuccEdt->F->getName() << "\n");
//           // /// Add dependency
//           // CallerEdt->Successors.push_back(SuccEdt);
//           // SuccEdt->Predecessors.push_back(CallerEdt);
//         } break;
//         default:
//           break;
//       }
//       /// Get the successor EDT
//       // EdtInfo *SuccEdt = nullptr;
//       // if(CalleeEdt->Type == EdtInfo::PARALLEL) {
//       //   SuccEdt = CalleeEdt->Successors[0];
//       //   LLVM_DEBUG(dbgs() << "    - Succ: " << SuccEdt->F->getName() << "\n");
//       // }

//     }
//   }

// //   DominatorTree *DT = AG.getAnalysis<DominatorTreeAnalysis>(*F);
// //   // auto &MSSA = AG.getAnalysis<MemorySSAAnalysis>(*F, false)->getMSSA();
// //   auto *MD = AG.getAnalysis<MemoryDependenceAnalysis>(*F);
// // // MemorySSA &MSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();
// //   LLVM_DEBUG(dbgs()<<*F<<"\n");
// //   // MSSA.dump();

  

// //     /// Iterate through the arguments of the call
// //     for(uint32_t ArgItr = 0; ArgItr < CB->data_operands_size(); ArgItr++) {
// //       Value *Arg = CB->getArgOperand(ArgItr);
// //       LLVM_DEBUG(dbgs() << "     -- Arg: " << *Arg << "\n");
// //       /// We are only concerned with instructions
// //       Instruction *ArgInst = dyn_cast<Instruction>(Arg);
// //       if(!ArgInst)
// //         continue;
// //       /// Is used after the BB?
// //       if(!ArgInst->isUsedOutsideOfBlock(ArgInst->getParent()))
// //         continue;
// //       LLVM_DEBUG(dbgs() << "     -- It is used outside the BB\n");
// //       /// Get Dep
// //       MemDepResult Dep = MD->getDependency(ArgInst);
// //       if(Dep.getInst())
// //         LLVM_DEBUG(dbgs() << "     -- Dep: " << *Dep.getInst() << "\n");
// //       // const MemoryDependenceResults::NonLocalDepInfo &Deps = MD->getNonLocalPointerDependency(Arg, CB->getParent());
// //       // for (const NonLocalDepEntry &I : Deps) {
// //         // LLVM_DEBUG(dbgs() << "     -- NonLocalDepBB: " << I.getBB()->getName() << "\n");
// //       // }
// //     }

// //     // MD->getDependency(CB);
// //     // MemDepResult Dep = MD->getDependency(CB);
// //     // Instruction *DepInst = Dep.getInst();
// //     // if(!DepInst)
// //     //   continue;
// //     // LLVM_DEBUG(dbgs() << "     -- Dep: " << *DepInst << "\n");

// //     // // Non-local case.
// //     // const MemoryDependenceResults::NonLocalDepInfo &deps =
// //     //   MD->getNonLocalCallDependency(CB);
// //     // // FIXME: Move the checking logic to MemDep!
// //     // CallInst* cdep = nullptr;
// //     // // Check to see if we have a single dominating call instruction that is
// //     // // identical to C.
// //     // for (const NonLocalDepEntry &I : deps) {
// //     //   LLVM_DEBUG(dbgs() << "     -- NonLocalDepBB: " << I.getBB()->getName() << "\n");
// //     //   // LLVM_DEBUG(dbgs() << "     -- NonLocalDepEntry: " << *I.getResult().getInst() << "\n");
// //     //   // if (I.getResult().isNonLocal())
// //     //   //   continue;

// //     //   // // We don't handle non-definitions.  If we already have a call, reject
// //     //   // // instruction dependencies.
// //     //   // if (!I.getResult().isDef() || cdep != nullptr) {
// //     //   //   cdep = nullptr;
// //     //   //   break;
// //     //   // }

// //     //   // CallInst *NonLocalDepCall = dyn_cast<CallInst>(I.getResult().getInst());
// //     //   // // FIXME: All duplicated with non-local case.
// //     //   // if (NonLocalDepCall && DT->properlyDominates(I.getBB(), C->getParent())) {
// //     //   //   cdep = NonLocalDepCall;
// //     //   //   continue;
// //     //   // }

// //     //   // cdep = nullptr;
// //     //   // break;
// //     // }

// //   }

// //   // }
// // }

// // LLVM_DEBUG(dbgs() << "\n---------------------------------\n");
// // We do a bottom-up SCC traversal of the call graph.  In other words, we
// // visit all callees before callers (leaf-first).
// // for (scc_iterator<CallGraph *> I = scc_begin(&CG); !I.isAtEnd(); ++I) {
// //   const std::vector<CallGraphNode *> &SCC = *I;
// //   assert(!SCC.empty() && "SCC with no functions?");

// //   // Skip external node SCCs.
// //   Function *F = SCC[0]->getFunction();
// //   if (!F || !F->isDefinitionExact())
// //     continue;
// //   /// Get corresponding EDT
// //   EdtInfo *EdtInfo = AT.getEdt(F);
// //   if(!EdtInfo)
// //     continue;


// //   /// Debug SCC
// //   LLVM_DEBUG(dbgs() << TAG << "SCC: \n");
// //   /// Called Functions
// //   LLVM_DEBUG(for (CallGraphNode *CGN : SCC) {
// //     // CGN->getFunction()
// //     // if (!SCCI.hasCycle()) // We only care about cycles, not standalone nodes.
// //     // continue;
// //     dbgs() << TAG << "  - " << CGN->getFunction()->getName() << "\n";
// //     /// DEBUG CGN
// //     CGN->dump();
    
// //   });
// // }
//   /// Analyze EDTs
//   // for(auto Itr : AT.`orFunction) {
//     // auto *F = Itr.first;
//     // auto &EdtInfo = Itr.second;
//     // /// Analyze Data Environment
//     // for (auto &Arg : F->args()) {
//     //   Type *ArgType = Arg.getType();
//     //   /// For now assume that if it is a pointer, it is a shared variable
//     //   if (PointerType *PT = dyn_cast<PointerType>(ArgType))
//     //     AT.insertArgToDE(&Arg, DataEnv::SHARED, EdtInfo);
//     //   /// If not, it is a first private variable
//     //   else
//     //     AT.insertArgToDE(&Arg, DataEnv::FIRSTPRIVATE, EdtInfo);
//     // }
//     // LLVM_DEBUG(dbgs() << EdtInfo.DE << "\n");
//   // }
// }
