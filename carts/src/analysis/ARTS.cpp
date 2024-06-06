
#include "noelle/core/Noelle.hpp"

#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSUtils.h"
// #include "noelle/core/Task.hpp"

using namespace llvm;
using namespace arts;
using namespace arts::utils;
// using namespace arts_utils::omp;
// using namespace arcana::noelle;

/// DEBUG
#define DEBUG_TYPE "arts"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// EDT Environment
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }

/// EDT Cache
// EDTCache::EDTCache(Module &M, noelle::Noelle &NM) : M(M), NM(NM) {}

EDTCache::~EDTCache() {}

void EDTCache::insertEDT(Value *V, EDT *E) { Values[V].insert(E); }

bool EDTCache::isValueInEDT(Value *V, EDT *E) {
  auto Itr = Values.find(V);
  if (Itr == Values.end())
    return false;
  return Itr->second.count(E);
}

SetVector<EDT *> EDTCache::getEDTs(Value *V) const {
  auto Itr = Values.find(V);
  if (Itr == Values.end())
    return SetVector<EDT *>();
  return Itr->second;
}

/// EDT Task
// void EDTTask::removeDeadInstructions() {
//   LLVM_DEBUG(dbgs() << TAG << "Deleting dead instructions in EDT: "
//                     << F->getName() << "\n");
//   SmallVector<Value *, 16> ValuesToRemove;
//   for (auto &BB : *F) {
//     for (auto &I : BB) {
//       /// If the instruction is trivially dead, remove it
//       if (isInstructionTriviallyDead(&I)) {
//         LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
//         ValuesToRemove.push_back(&I);
//         continue;
//       }
//       /// However, if the instruction is not trivially dead, check its operands
//       /// If any of them is undef, remove the instruction
//       for (auto &Op : I.operands()) {
//         if (isa<UndefValue>(Op.get())) {
//           LLVM_DEBUG(dbgs() << TAG << "- Removing: " << I << "\n");
//           ValuesToRemove.push_back(&I);
//           break;
//         }
//       }
//     }
//   }
//   removeValues(ValuesToRemove);
// }

// void EDTTask::cloneAndAddBasicBlocks(Function *F) {
//   for (auto &BB : *F)
//     cloneAndAddBasicBlock(&BB);
// }

// void EDTTask::cloneAndAddBasicBlocks(BlockSequence &BBs) {
//   for (auto BB : BBs)
//     cloneAndAddBasicBlock(BB);
// }

/// EDT
void EDT::insertValueToEnv(Value *Val) {
  /// Pointer is a depv, else, it is a paramv
  if (PointerType *PT = dyn_cast<PointerType>(Val->getType())) {
    Cache.insertEDT(Val, this);
    Env.insertDepV(Val);
  } else
    Env.insertParamV(Val);
  /// TODO: Add extra info to OpenMP Frontend to have info about
  /// Data Sharing attributes
}

void EDT::insertValueToEnv(Value *Val, bool IsDepV) {
  /// Pointer is a depv, else, it is a paramv
  if (IsDepV) {
    Cache.insertEDT(Val, this);
    Env.insertDepV(Val);
  } else
    Env.insertParamV(Val);
}

/// Parallel EDT
ParallelEDT::ParallelEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache) {
  // OutlinedFnCall = CB;
  // OutlinedFn = omp::getOutlinedFunction(CB);
  // setDataEnv(CB);
}

void ParallelEDT::createTask() {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task for ParallelEDT\n");
}

void ParallelEDT::setDataEnv(CallBase *CB) {
  // assert(OutlinedFn && "Outlined function not found");
  // LLVM_DEBUG(dbgs() << TAG << "Setting data environment for ParallelEDT\n");
  // const Data &RTI = omp::getRTData(omp::getRTFunction(*CB));
  // Use *CallArgItr = CB->arg_begin() + RTI.KeepCallArgsFrom;
  // Argument *FnArgItr = OutlinedFn->arg_begin() + RTI.KeepFnArgsFrom;
  // for (auto ArgNum = RTI.KeepFnArgsFrom; ArgNum < OutlinedFn->arg_size();
  //      ++CallArgItr, ++FnArgItr, ++ArgNum) {
  //   insertValueToEnv(*CallArgItr);
  //   RewiringMap[FnArgItr] = *CallArgItr;
  // }
}

/// ParallelDone EDT
ParallelDoneEDT::ParallelDoneEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache) {
  setDataEnv(CB);

}

void ParallelDoneEDT::createTask() {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task for ParallelDoneEDT\n");
}

void ParallelDoneEDT::setDataEnv(CallBase *CB) {
  // LLVM_DEBUG(dbgs() << TAG
  //                   << "Setting data environment for ParallelDoneEDT\n");
  // auto *SplitInst = CB->getNextNonDebugInstruction();

  // /// Split block at Call to EDT
  // LoopInfo *LI = nullptr;
  // DominatorTree *DT = nullptr;
  // BasicBlock *DoneBB = SplitBlock(CB->getParent(), SplitInst, DT, LI);

  // /// Create DoneEDT
  // auto &NM = Cache.getNoelle();
  // auto &NDT = NM.getDominators(CB->getFunction())->DT;
  // auto Region = NDT.getDescendants(DoneBB);
  // BlockSequence RegionSeq;
  // for (auto *BB : Region)
  //   RegionSeq.push_back(BB);

  // /// Set DataEnvironment
  // CodeExtractor CE(RegionSeq);
  // SetVector<Value *> Inputs, Outputs, Sinks;
  // CE.findInputsOutputs(Inputs, Outputs, Sinks);
  // for (auto *I : Inputs)
  //   insertValueToEnv(I);
  // LLVM_DEBUG(dbgs() << TAG << " - DoneEDT OutlinedFn: " << OutlinedFn->getName()
  //                   << "\n");
  // LLVM_DEBUG(dbgs() << TAG << " - DoneEDT OutlinedFnCall: " << *OutlinedFnCall
  //                   << "\n");
}

void ParallelDoneEDT::createOutlinedFn(CallBase *CB) {
  /// Extract code region
  // CodeExtractorAnalysisCache CEAC(*CB->getFunction());
  // /// Set OutlinedFn info
  // OutlinedFn = CE.extractCodeRegion(CEAC);
  // /// Get Call to OutlinedFn
  // for(auto &U : OutlinedFn->uses()) {
  //   if (auto *CI = dyn_cast<CallInst>(U.getUser())) {
  //     if (CI->getCalledFunction() == OutlinedFn) {
  //       OutlinedFnCall = CI;
  //       break;
  //     }
  //   }
  // }
  // assert(OutlinedFnCall && "Call to OutlinedFn not found");
}

/// Task EDT
TaskEDT::TaskEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache) {
  // OutlinedFnCall = CB;
  // OutlinedFn = omp::getOutlinedFunction(CB);
  // setDataEnv(CB);
}

void TaskEDT::createTask() {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task for TaskEDT\n");
}

void TaskEDT::setDataEnv(CallBase *CB) {
  // LLVM_DEBUG(dbgs() << TAG << " - Setting data environment for TaskEDT\n");
  // /// Analyze return pointer
  // /// For the shared variables we are interested in all stores that are done
  // /// to the shareds field of the kmp_task_t struct. For the firstprivate
  // /// variables we are interested in all stores that are done to the
  // /// privates fields of the kmp_task_t_with_privates struct.
  // ///
  // /// The returned Val is a pointer to the
  // /// kmp_task_t_with_privates struct.
  // /// struct kmp_task_t_with_privates {
  // ///    kmp_task_t task_data;
  // ///    kmp_privates_t privates;
  // /// };
  // /// typedef struct kmp_task {
  // ///   void *shareds;
  // ///   kmp_routine_entry_t routine;
  // ///   kmp_int32 part_id;
  // ///   kmp_cmplrdata_t data1;
  // ///   kmp_cmplrdata_t data2;
  // /// } kmp_task_t;
  // ///
  // /// - For shared variables, the access of the shareds field is obtained by
  // ///   obtaining stores done to offset 0 of the returned Val of the
  // ///   taskalloc.
  // /// - For firstprivate variables, the access of the privates field is
  // ///   obtained by obtaining stores done to offset 8 of the returned Val of the
  // ///   kmp_task_t_with_privates struct.
  // /// Get the size of the kmp_task_t struct
  // const DataLayout &DL = CB->getModule()->getDataLayout();
  // LLVMContext &Ctx = CB->getContext();
  // const auto *TaskStruct = dyn_cast<StructType>(CB->getType());
  // const auto TaskDataSize = static_cast<int64_t>(
  //     DL.getTypeAllocSize(TaskStruct->getTypeByName(Ctx, "struct.kmp_task_t")));

  // /// Analyze Task Data
  // /// This analysis assumes we only have stores to the task struct
  // /// Get offsets and values from the Task Data - Call to __kmpc_omp_task_alloc
  // DenseMap<Value *, int64_t> ValueToOffsetTD;
  // Instruction *CurI = CB;
  // do {
  //   if (auto *SI = dyn_cast<StoreInst>(CurI)) {
  //     int64_t Offset = -1;
  //     auto *Val = SI->getValueOperand();
  //     GetPointerBaseWithConstantOffset(SI->getPointerOperand(), Offset, DL);
  //     ValueToOffsetTD[Val] = Offset;
  //     /// Private variables
  //     if (Offset >= TaskDataSize) {
  //       insertValueToEnv(Val, false);
  //       continue;
  //     }
  //     /// Shared variables
  //     insertValueToEnv(Val, true);
  //     continue;
  //   }
  //   /// Break if we find a call to a task function
  //   if (auto *CI = dyn_cast<CallInst>(CurI)) {
  //     if (omp::isTaskFunction(*CI))
  //       break;
  //   }
  // } while ((CurI = CurI->getNextNonDebugInstruction()));

  // /// Rewrite Task Outlined Function arguments to match the Task Data
  // auto *OutlinedFn = omp::getOutlinedFunction(CB);
  // Argument *TaskData = dyn_cast<Argument>(OutlinedFn->arg_begin() + 1);
  // /// This assumes the 'desaggregation' happens in the first basic block
  // BasicBlock &EntryBB = OutlinedFn->getEntryBlock();
  // auto *TaskDataPtr = &*(++EntryBB.begin());
  // /// Get offsets and values from the Task Data - Task Outlined function
  // DenseMap<int64_t, Value *> OffsetToValueOF;
  // CurI = &*EntryBB.begin();
  // while ((CurI = CurI->getNextNonDebugInstruction())) {
  //   if (!isa<LoadInst>(CurI))
  //     continue;

  //   auto *L = cast<LoadInst>(CurI);
  //   auto *Val = L->getPointerOperand();
  //   int64_t Offset = -1;
  //   auto *BasePointer = GetPointerBaseWithConstantOffset(Val, Offset, DL);

  //   if (Offset == -1)
  //     continue;

  //   bool Cond = (Offset >= TaskDataSize && BasePointer == TaskData) ||
  //               (Offset < TaskDataSize && BasePointer == TaskDataPtr);
  //   if (!Cond)
  //     continue;

  //   OffsetToValueOF[Offset] = L;
  //   if (OffsetToValueOF.size() == ValueToOffsetTD.size())
  //     break;
  // }
  // assert(ValueToOffsetTD.size() == OffsetToValueOF.size() &&
  //        "ValueToOffsetTD and ValueToOffsetOF have different sizes");

  // /// Preprocessing
  // DenseMap<Value *, Value *> RewiringMap;
  // for (auto Itr : ValueToOffsetTD) {
  //   auto *From = Itr.first;
  //   auto *To = OffsetToValueOF[Itr.second];
  //   RewiringMap[To] = From;
  // }
}

/// Main EDT
void MainEDT::createTask() {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task for TaskEDT\n");
}

void MainEDT::setDataEnv(CallBase *CB) {}