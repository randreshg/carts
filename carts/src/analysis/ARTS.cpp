#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"
#include <cstdint>

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSMetadata.h"

using namespace llvm;
using namespace arts;
using namespace arts::types;

/// DEBUG
#define DEBUG_TYPE "arts"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                            ART TYPES                                ///
/// ------------------------------------------------------------------- ///
namespace arts::types {
Twine toString(EDTType Ty) {
  switch (Ty) {
  case EDTType::Parallel:
    return "parallel";
  case EDTType::Task:
    return "task";
  case EDTType::Main:
    return "main";
  default:
    break;
  }
  return "unknown";
}

Twine toString(EDTArgType Ty) {
  switch (Ty) {
  case EDTArgType::Param:
    return "param";
  case EDTArgType::Dep:
    return "dep";
  default:
    break;
  }
  return "unknown";
}

EDTType toEDTType(StringRef Str) {
  if (Str == "parallel")
    return EDTType::Parallel;
  if (Str == "task")
    return EDTType::Task;
  if (Str == "main")
    return EDTType::Main;
  return EDTType::Unknown;
}

EDTArgType toEDTArgType(StringRef Str) {
  if (Str == "param")
    return EDTArgType::Param;
  if (Str == "dep")
    return EDTArgType::Dep;
  return EDTArgType::Unknown;
}
} // namespace arts::types

/// EDT Cache
EDTCache::EDTCache(Module &M) : M(M) {}
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

/// ------------------------------------------------------------------- ///
///                          DATA ENVIRONMENT                           ///
/// ------------------------------------------------------------------- ///
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }

/// ------------------------------------------------------------------- ///
///                                 EDT                                 ///
/// ------------------------------------------------------------------- ///
uint32_t EDT::ID = 0;
EDT::EDT(EDTCache &Cache, EDTMetadata &MD, CallBase *Call)
    : Cache(Cache), MD(MD), Env(this), Call(Call) {
  ID++;
}

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

CallBase *EDT::getCall() { return Call; }
Function *EDT::getFn() { return &MD.getFunction(); }
EDTEnvironment &EDT::getDataEnv() { return Env; }
Twine EDT::getName() { return getFn()->getName(); }
uint32_t EDT::getID() { return ID; }

/// Parallel EDT
// ParallelEDT::ParallelEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache) {
//   // Call = CB;
//   // Fn = omp::getOutlinedFunction(CB);
//   // setDataEnv(CB);
// }

// void ParallelEDT::setDataEnv(CallBase *CB) {
//   // assert(Fn && "Outlined function not found");
//   // LLVM_DEBUG(dbgs() << TAG << "Setting data environment for
//   ParallelEDT\n");
//   // const Data &RTI = omp::getRTData(omp::getRTFunction(*CB));
//   // Use *CallArgItr = CB->arg_begin() + RTI.KeepCallArgsFrom;
//   // Argument *FnArgItr = Fn->arg_begin() + RTI.KeepFnArgsFrom;
//   // for (auto ArgNum = RTI.KeepFnArgsFrom; ArgNum < Fn->arg_size();
//   //      ++CallArgItr, ++FnArgItr, ++ArgNum) {
//   //   insertValueToEnv(*CallArgItr);
//   //   RewiringMap[FnArgItr] = *CallArgItr;
//   // }
// }

// /// ParallelDone EDT
// ParallelDoneEDT::ParallelDoneEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache)
// {
//   setDataEnv(CB);
// }

// void ParallelDoneEDT::createTask() {
//   LLVM_DEBUG(dbgs() << TAG << "Creating Task for ParallelDoneEDT\n");
// }

// void ParallelDoneEDT::setDataEnv(CallBase *CB) {
//   // LLVM_DEBUG(dbgs() << TAG
//   //                   << "Setting data environment for ParallelDoneEDT\n");
//   // auto *SplitInst = CB->getNextNonDebugInstruction();

//   // /// Split block at Call to EDT
//   // LoopInfo *LI = nullptr;
//   // DominatorTree *DT = nullptr;
//   // BasicBlock *DoneBB = SplitBlock(CB->getParent(), SplitInst, DT, LI);

//   // /// Create DoneEDT
//   // auto &NM = Cache.getNoelle();
//   // auto &NDT = NM.getDominators(CB->getFunction())->DT;
//   // auto Region = NDT.getDescendants(DoneBB);
//   // BlockSequence RegionSeq;
//   // for (auto *BB : Region)
//   //   RegionSeq.push_back(BB);

//   // /// Set DataEnvironment
//   // CodeExtractor CE(RegionSeq);
//   // SetVector<Value *> Inputs, Outputs, Sinks;
//   // CE.findInputsOutputs(Inputs, Outputs, Sinks);
//   // for (auto *I : Inputs)
//   //   insertValueToEnv(I);
//   // LLVM_DEBUG(dbgs() << TAG << " - DoneEDT Fn: " <<
//   // Fn->getName()
//   //                   << "\n");
//   // LLVM_DEBUG(dbgs() << TAG << " - DoneEDT Call: " <<
//   // *Call
//   //                   << "\n");
// }

// void ParallelDoneEDT::createFn(CallBase *CB) {
//   /// Extract code region
//   // CodeExtractorAnalysisCache CEAC(*CB->getFunction());
//   // /// Set Fn info
//   // Fn = CE.extractCodeRegion(CEAC);
//   // /// Get Call to Fn
//   // for(auto &U : Fn->uses()) {
//   //   if (auto *CI = dyn_cast<CallInst>(U.getUser())) {
//   //     if (CI->getCalledFunction() == Fn) {
//   //       Call = CI;
//   //       break;
//   //     }
//   //   }
//   // }
//   // assert(Call && "Call to Fn not found");
// }

// /// Task EDT
// TaskEDT::TaskEDT(EDTCache &Cache, CallBase *CB) : EDT(Cache) {
//   // Call = CB;
//   // Fn = omp::getOutlinedFunction(CB);
//   // setDataEnv(CB);
// }

// void TaskEDT::createTask() {
//   LLVM_DEBUG(dbgs() << TAG << "Creating Task for TaskEDT\n");
// }

// void TaskEDT::setDataEnv(CallBase *CB) {
//   // LLVM_DEBUG(dbgs() << TAG << " - Setting data environment for
//   TaskEDT\n");
//   // /// Analyze return pointer
//   // /// For the shared variables we are interested in all stores that are
//   done
//   // /// to the shareds field of the kmp_task_t struct. For the firstprivate
//   // /// variables we are interested in all stores that are done to the
//   // /// privates fields of the kmp_task_t_with_privates struct.
//   // ///
//   // /// The returned Val is a pointer to the
//   // /// kmp_task_t_with_privates struct.
//   // /// struct kmp_task_t_with_privates {
//   // ///    kmp_task_t task_data;
//   // ///    kmp_privates_t privates;
//   // /// };
//   // /// typedef struct kmp_task {
//   // ///   void *shareds;
//   // ///   kmp_routine_entry_t routine;
//   // ///   kmp_int32 part_id;
//   // ///   kmp_cmplrdata_t data1;
//   // ///   kmp_cmplrdata_t data2;
//   // /// } kmp_task_t;
//   // ///
//   // /// - For shared variables, the access of the shareds field is obtained
//   by
//   // ///   obtaining stores done to offset 0 of the returned Val of the
//   // ///   taskalloc.
//   // /// - For firstprivate variables, the access of the privates field is
//   // ///   obtained by obtaining stores done to offset 8 of the returned Val
//   of
//   // the
//   // ///   kmp_task_t_with_privates struct.
//   // /// Get the size of the kmp_task_t struct
//   // const DataLayout &DL = CB->getModule()->getDataLayout();
//   // LLVMContext &Ctx = CB->getContext();
//   // const auto *TaskStruct = dyn_cast<StructType>(CB->getType());
//   // const auto TaskDataSize = static_cast<int64_t>(
//   //     DL.getTypeAllocSize(TaskStruct->getTypeByName(Ctx,
//   //     "struct.kmp_task_t")));

//   // /// Analyze Task Data
//   // /// This analysis assumes we only have stores to the task struct
//   // /// Get offsets and values from the Task Data - Call to
//   // __kmpc_omp_task_alloc DenseMap<Value *, int64_t> ValueToOffsetTD;
//   // Instruction *CurI = CB;
//   // do {
//   //   if (auto *SI = dyn_cast<StoreInst>(CurI)) {
//   //     int64_t Offset = -1;
//   //     auto *Val = SI->getValueOperand();
//   //     GetPointerBaseWithConstantOffset(SI->getPointerOperand(), Offset,
//   DL);
//   //     ValueToOffsetTD[Val] = Offset;
//   //     /// Private variables
//   //     if (Offset >= TaskDataSize) {
//   //       insertValueToEnv(Val, false);
//   //       continue;
//   //     }
//   //     /// Shared variables
//   //     insertValueToEnv(Val, true);
//   //     continue;
//   //   }
//   //   /// Break if we find a call to a task function
//   //   if (auto *CI = dyn_cast<CallInst>(CurI)) {
//   //     if (omp::isTaskFunction(*CI))
//   //       break;
//   //   }
//   // } while ((CurI = CurI->getNextNonDebugInstruction()));

//   // /// Rewrite Task Outlined Function arguments to match the Task Data
//   // auto *Fn = omp::getOutlinedFunction(CB);
//   // Argument *TaskData = dyn_cast<Argument>(Fn->arg_begin() + 1);
//   // /// This assumes the 'desaggregation' happens in the first basic block
//   // BasicBlock &EntryBB = Fn->getEntryBlock();
//   // auto *TaskDataPtr = &*(++EntryBB.begin());
//   // /// Get offsets and values from the Task Data - Task Outlined function
//   // DenseMap<int64_t, Value *> OffsetToValueOF;
//   // CurI = &*EntryBB.begin();
//   // while ((CurI = CurI->getNextNonDebugInstruction())) {
//   //   if (!isa<LoadInst>(CurI))
//   //     continue;

//   //   auto *L = cast<LoadInst>(CurI);
//   //   auto *Val = L->getPointerOperand();
//   //   int64_t Offset = -1;
//   //   auto *BasePointer = GetPointerBaseWithConstantOffset(Val, Offset, DL);

//   //   if (Offset == -1)
//   //     continue;

//   //   bool Cond = (Offset >= TaskDataSize && BasePointer == TaskData) ||
//   //               (Offset < TaskDataSize && BasePointer == TaskDataPtr);
//   //   if (!Cond)
//   //     continue;

//   //   OffsetToValueOF[Offset] = L;
//   //   if (OffsetToValueOF.size() == ValueToOffsetTD.size())
//   //     break;
//   // }
//   // assert(ValueToOffsetTD.size() == OffsetToValueOF.size() &&
//   //        "ValueToOffsetTD and ValueToOffsetOF have different sizes");

//   // /// Preprocessing
//   // DenseMap<Value *, Value *> RewiringMap;
//   // for (auto Itr : ValueToOffsetTD) {
//   //   auto *From = Itr.first;
//   //   auto *To = OffsetToValueOF[Itr.second];
//   //   RewiringMap[To] = From;
//   // }
// }

// /// Main EDT
// void MainEDT::createTask() {
//   LLVM_DEBUG(dbgs() << TAG << "Creating Task for TaskEDT\n");
// }

// void MainEDT::setDataEnv(CallBase *CB) {}