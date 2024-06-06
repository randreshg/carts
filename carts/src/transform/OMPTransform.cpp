// Description: Main file for the Compiler for ARTS (OmpTransform) pass.

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/Support/Debug.h"
#include <cstdint>

#include "carts/analysis/ARTS.h"
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
  /// Get the set of analyzable functions in the module
  for (Function &F : M) {
    if (F.isDeclaration() && !F.hasLocalLinkage())
      continue;
    Functions.insert(&F);
  }
  identifyEDTs();
  return true;
}

void OMPTransform::identifyEDTs() {
  /// Identify EDTs in the module
  for (auto &F : Functions) {
    identifyEDTs(*F);
  }
}

void OMPTransform::handleParallelRegion(CallBase &CB) {
  EDTIRBuilder IRB(EDTType::Parallel);
  auto *OutlinedFn = getOutlinedFunction(&CB);
  const OMPData &RTI = getRTData(getRTFunction(CB));
  Use *CallArgItr = CB.arg_begin() + RTI.KeepCallArgsFrom;
  Argument *FnArgItr = OutlinedFn->arg_begin() + RTI.KeepFnArgsFrom;
  LLVM_DEBUG(dbgs() << TAG << "Outlined Function: " << OutlinedFn->getName()
                    << "\n");
  for (uint32_t ArgNum = RTI.KeepFnArgsFrom; ArgNum < OutlinedFn->arg_size();
       ++CallArgItr, ++FnArgItr, ++ArgNum) {
    Value *CallV = *CallArgItr;
    LLVM_DEBUG(dbgs() << TAG << " - Inserting value: " << *CallV << "\n");
    if (PointerType *PT = dyn_cast<PointerType>(CallV->getType()))
      IRB.insertDep(CallV, FnArgItr);
    else
      IRB.insertParam(CallV, FnArgItr);
  }
  /// Build the EDT
  IRB.buildEDT(M, &CB, OutlinedFn);
}

void OMPTransform::handleTaskRegion(CallBase &CB) {
  // EDTIRBuilder IRB(EDTType::Task);
  // auto *OutlinedFn = getOutlinedFunction(&CB);
  // const OMPData &RTI = getRTData(getRTFunction(CB));
  // Use *CallArgItr = CB.arg_begin() + RTI.KeepCallArgsFrom;
  // Argument *FnArgItr = OutlinedFn->arg_begin() + RTI.KeepFnArgsFrom;
  // for (uint32_t ArgNum = 0; ArgNum < OutlinedFn->arg_size();
  //      ++CallArgItr, ++FnArgItr, ++ArgNum) {
  //   if (ArgNum < RTI.KeepCallArgsFrom) {
  //     IRB.insertUnusedArg(FnArgItr);
  //     continue;
  //   }
  //   /// Insert values
  //   Value *CallV = *CallArgItr;
  //   if (PointerType *PT = dyn_cast<PointerType>(CallV->getType()))
  //     IRB.insertDep(CallV, FnArgItr);
  //   else
  //     IRB.insertParam(CallV, FnArgItr);
  // }
  // /// Build the EDT
  // IRB.buildEDT(M, &CB, OutlinedFn);
}

void OMPTransform::identifyEDTs(Function &F) {
  /// If the function is not analyzable, return
  if (!Functions.count(&F))
    return;
  LLVM_DEBUG(dbgs() << "\n- - - - - - - - - - - - - - - - - - - - - - - -\n");
  LLVM_DEBUG(dbgs() << TAG << "Processing function: " << F.getName() << "\n");
  removeLifetimeMarkers(F);
  for (auto &Inst : instructions(&F)) {
    auto *CB = dyn_cast<CallBase>(&Inst);
    if (!CB)
      continue;
    /// Get the callee
    OMPType RTF = getRTFunction(*CB);
    switch (RTF) {
    case OMPType::PARALLEL: {
      LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
      LLVM_DEBUG(dbgs() << TAG << "Parallel Region Found:\n" << *CB << "\n");
      handleParallelRegion(*CB);
    } break;
    case OMPType::TASKALLOC: {
      LLVM_DEBUG(dbgs() << "- - - - - - - - - - - - - - - -\n");
      LLVM_DEBUG(dbgs() << TAG << "Task Region Found:\n" << *CB << "\n");
      // handleTaskRegion(*CB);
      // insertNode(new TaskEDT(Cache, CB), ParentEDTNode);
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
  }
}

/// ------------------------------------------------------------------- ///
///                        OMPTransformPass                             ///
/// ------------------------------------------------------------------- ///
PreservedAnalyses OMPTransformPass::run(Module &M, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "Running OmpTransformPass on Module: \n"
                    << M.getName() << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  OMPTransform OT(M);
  OT.run(AM);

  /// Print module info
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  LLVM_DEBUG(dbgs() << TAG << "OmpTransformPass has finished\n\n" << M << "\n");
  LLVM_DEBUG(dbgs() << "\n-------------------------------------------------\n");
  return PreservedAnalyses::all();
}

// This part is the new way of registering your pass
extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "OMPTransform", "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "omp-transform") {
                    FPM.addPass(OMPTransformPass());
                    return true;
                  }
                  return false;
                });
          }};
}

/// ------------------------------------------------------------------- ///
/// ------------------------------ OMP -------------------------------- ///
/// ------------------------------------------------------------------- ///
namespace arts::omp {
Function *getOutlinedFunction(CallBase *Call) {
  auto Data = getRTData(getRTFunction(*Call));
  return dyn_cast<Function>(
      Call->getArgOperand(Data.OutlinedFnPos)->stripPointerCasts());
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

OMPType getRTFunction(Function *F) {
  if (!F)
    return OMPType::OTHER;
  auto CalleeName = F->getName();
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

bool isTaskFunction(Function *F) {
  auto RT = getRTFunction(F);
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
} // namespace omp