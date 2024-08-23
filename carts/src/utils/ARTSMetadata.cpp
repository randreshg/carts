
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
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
  Function *Fn = Builder.getNewFn();
  assert(Fn && "Function is null");

  LLVMContext &Ctx = Fn->getContext();
  /// Metadata Nodes for Argument Values
  SmallVector<Metadata *, 16> ArgMDs;
  auto &CallArgs = Builder.getCallArgs();
  for (Value *CallArg : CallArgs) {
    EDTArgType ArgTy = Builder.getArgType(CallArg);
    string MDStr = toString(ArgTy).str();
    ArgMDs.push_back(MDString::get(Ctx, MDStr));
  }
  MDNode *ArgNode = MDNode::get(Ctx, ArgMDs);
  /// Metadata Node for EDT
  SmallVector<Metadata *, 16> EDTMDs;
  string EDTTyStr = toString(Builder.getEDTType()).str();
  EDTMDs.push_back(MDString::get(Ctx, EDTTyStr));
  EDTMDs.push_back(ArgNode);
  /// Set specific metadata for the function
  if (Fn->hasMetadata(CARTS_MD))
    Fn->setMetadata(CARTS_MD, nullptr);
  Fn->setMetadata(CARTS_MD, MDNode::get(Ctx, EDTMDs));
}

void TaskEDT::setMetadata(EDTIRBuilder &Builder, int32_t ThreadNum) {
  EDT::setMetadata(Builder);
  /// TODO: Add the number of threads...
}

void MainEDT::setMetadata(EDTIRBuilder &Builder) {
  TaskEDT::setMetadata(Builder);
}

void SyncEDT::setMetadata(EDTIRBuilder &Builder, SetVector<EDT *> &Inputs,
                          SetVector<EDT *> &Outputs, bool SyncChilds,
                          bool SyncDescendants, int32_t ThreadNum) {
  TaskEDT::setMetadata(Builder, ThreadNum);
  /// TODO: Add the inputs and outputs, and the sync flags...
}

void ParallelEDT::setMetadata(EDTIRBuilder &Builder, int32_t ThreadNum,
                              uint32_t NumThreads) {
  SetVector<EDT *> Inputs, Outputs;
  EDT::setMetadata(Builder);
  /// TODO: Add the number of threads...
}
