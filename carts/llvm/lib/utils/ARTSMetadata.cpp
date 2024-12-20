
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "llvm/IR/Function.h"
// #include "llvm/Support/Debug.h"
#include <cassert>
#include <cstdint>
#include <string>

/// DEBUG
#define DEBUG_TYPE "carts-metadata"
// #if !defined(NDEBUG)
// static constexpr auto TAG = "[" DEBUG_TYPE "] ";
// #endif

/// Metadata
using namespace arts;
using namespace llvm;
using namespace arts::types;

void EDT::setMetadata(EDTIRBuilder &Builder) {
  /// The EDT Metadata is composed of two parts:
  /// - The EDT Type ("Task", "Main", "Sync", "Parallel")
  /// - The EDT Arguments ("Param" or "Dep")
  Function *Fn = Builder.getNewFn();
  assert(Fn && "Function is null");
  LLVMContext &Ctx = Fn->getContext();
  /// Metadata Node
  SmallVector<Metadata *, 16> EDTMDs;
  /// EDT Type
  string EDTTyStr = toString(Builder.getEDTType()).str();
  EDTMDs.push_back(MDString::get(Ctx, EDTTyStr));
  /// EDT Arguments
  SmallVector<Metadata *, 16> ArgMDs;
  SmallVector<Value *, 16> &CallArgs = Builder.getCallArgs();
  for (Value *CallArg : CallArgs) {
    EDTArgType ArgTy = Builder.getArgType(CallArg);
    string MDStr = toString(ArgTy).str();
    ArgMDs.push_back(MDString::get(Ctx, MDStr));
  }
  MDNode *ArgNode = MDNode::get(Ctx, ArgMDs);
  EDTMDs.push_back(ArgNode);

  /// Set specific metadata for the function
  if (Fn->hasMetadata(ARTS_MD))
    Fn->setMetadata(ARTS_MD, nullptr);
  Fn->setMetadata(ARTS_MD, MDNode::get(Ctx, EDTMDs));
}

void TaskEDT::setMetadata(EDTIRBuilder &Builder, SetVector<EDT *> &Inputs,
                          SetVector<EDT *> &Outputs, int32_t ThreadNum) {
  EDT::setMetadata(Builder);
  // MDNode *MD = Builder.getNewFn()->getMetadata(ARTS_MD);
  // assert(MD && "CARTS Metadata Node is null");
  // /// Add a new metadata node for the Task EDT
  // SmallVector<Metadata *, 16> TaskMDs;
  // TaskMDs.push_back(MD->getOperand(1).get());
  // /// Add the inputs and outputs
  // SmallVector<Metadata *, 16> InputsMDs;
  // for (EDT *E : Inputs)
  //   InputsMDs.push_back(ValueAsMetadata::get(E->getEnv()->getEnvValue()));
  // SmallVector<Metadata *, 16> OutputsMDs;
  // for (EDT *E : Outputs)
  //   OutputsMDs.push_back(ValueAsMetadata::get(E->getEnv()->getEnvValue()));
  // TaskMDs.push_back(MDNode::get(Builder.getNewFn()->getContext(), InputsMDs));
  // TaskMDs.push_back(MDNode::get(Builder.getNewFn()->getContext(), OutputsMDs));
  /// Set the metadata for the Task EDT
  // Builder.getNewFn()->setMetadata(ARTS_MD, MDNode::get(Builder.getNewFn()->getContext(), TaskMDs));
  /// TODO: Add the number of threads...
}

void MainEDT::setMetadata(EDTIRBuilder &Builder) {
  SetVector<EDT *> Inputs, Outputs;
  TaskEDT::setMetadata(Builder, Inputs, Outputs);
}

void SyncEDT::setMetadata(EDTIRBuilder &Builder, SetVector<EDT *> &Inputs,
                          SetVector<EDT *> &Outputs, bool SyncChilds,
                          bool SyncDescendants, int32_t ThreadNum) {
  TaskEDT::setMetadata(Builder, Inputs, Outputs, ThreadNum);
  /// TODO: Add the inputs and outputs, and the sync flags...
}

void ParallelEDT::setMetadata(EDTIRBuilder &Builder, int32_t ThreadNum,
                              uint32_t NumThreads) {
  SetVector<EDT *> Inputs, Outputs;
  EDT::setMetadata(Builder);
  /// TODO: Add the number of threads...
}
