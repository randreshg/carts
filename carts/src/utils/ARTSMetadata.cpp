
#include "carts/utils/ARTSMetadata.h"
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "llvm/Support/Debug.h"
#include <cassert>
#include <cstdint>
#include <string>

/// DEBUG
#define DEBUG_TYPE "carts-metadata"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// Metadata
using namespace arts;
using namespace llvm;
using namespace arts::types;

/// ------------------------------------------------------------------- ///
///                           EDT ARG METADATA                          ///
/// ------------------------------------------------------------------- ///
EDTArgMetadata::EDTArgMetadata(EDTArgType Ty, EDTValue *Val)
    : Ty(Ty), Val(Val) {}
EDTArgMetadata::~EDTArgMetadata() {}

string EDTArgMetadata::getName(EDTArgType Ty) {
  switch (Ty) {
  case EDTArgType::Dep:
    return (toString(EDTArgType::Dep)).str();
  case EDTArgType::Param:
    return (toString(EDTArgType::Param)).str();
  default:
    return "";
  }
}

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
// EDTMetadata::EDTMetadata(EDTType Ty, EDTFunction *Fn) : Ty(Ty), Fn(Fn) {
//   MDNode *MD = Fn->getMetadata(CARTS_MD);
//   assert(MD && "CARTS Metadata Node is null");
// }

// EDTMetadata::~EDTMetadata() {
//   for (auto *Arg : Args)
//     delete Arg;
// }

// string EDTMetadata::getName(EDTType Ty) {
//   switch (Ty) {
//   case EDTType::Task:
//     return (toString(EDTType::Task)).str();
//   case EDTType::Main:
//     return (toString(EDTType::Main)).str();
//     ;
//   case EDTType::Sync:
//     return (toString(EDTType::Sync)).str();
//     ;
//   case EDTType::Parallel:
//     return (toString(EDTType::Sync)).str();
//     ;
//   default:
//     return "";
//   }
// }

// EDTMetadata *EDTMetadata::getMetadata(EDTFunction *Fn) {
//   /// Check if the function has carts metadata
//   if (!Fn->hasMetadata(CARTS_MD)) {
//     LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
//                       << " doesn't have CARTS Metadata\n");
//     return nullptr;
//   }
//   LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
//                     << " has CARTS Metadata\n");
//   /// Get the metadata node
//   MDNode *MD = Fn->getMetadata(CARTS_MD);
//   assert(MD && "CARTS Metadata Node is null");
//   assert(MD->getNumOperands() > 0 && "CARTS Metadata Node is empty");
//   /// Get the EDT Type
//   auto *TyMD = dyn_cast<MDString>(MD->getOperand(0).get());
//   assert(TyMD && "EDT Type Metadata is null");
//   /// Get the EDT Metadata
//   EDTMetadata *EDTMD;
//   EDTType Ty = toEDTType(TyMD->getString());
//   switch (Ty) {
//   case EDTType::Task:
//     EDTMD = new TaskEDTMetadata(Fn);
//     break;
//   case EDTType::Main:
//     return new MainEDTMetadata(Fn);
//     break;
//   case EDTType::Sync:
//     EDTMD = new SyncEDTMetadata(Fn);
//     llvm_unreachable("SyncEDTMetadata not implemented");
//     break;
//   case EDTType::Parallel:
//     EDTMD = new ParallelEDTMetadata(Fn);
//     break;
//   default:
//     return nullptr;
//   }
//   assert(EDTMD && "EDT Metadata is null");

//   return EDTMD;
// }

void EDT::setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder) {
  LLVMContext &Ctx = Fn->getContext();
  /// Metadata Nodes for Argument Values
  SmallVector<Metadata *, 16> ArgMDs;
  for (auto *CallArg : Builder.CallArgs) {
    EDTArgType ArgTy = Builder.CallArgTypeMap[CallArg];
    std::string MDStr = EDTArgMetadata::getName(ArgTy);
    ArgMDs.push_back(MDString::get(Ctx, MDStr));
  }
  MDNode *ArgNode = MDNode::get(Ctx, ArgMDs);
  /// Metadata Node for EDT
  SmallVector<Metadata *, 16> EDTMDs;
  string EDTTyStr = EDTMetadata::getName(Builder.Ty);
  EDTMDs.push_back(MDString::get(Ctx, EDTTyStr));
  if (ArgMDs.size() > 0)
    EDTMDs.push_back(ArgNode);
  /// Set specific metadata for the function
  /// TODO: If it is parallel, add the number of threads...
  Fn->setMetadata(CARTS_MD, MDNode::get(Ctx, EDTMDs));
}

void TaskEDT::setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                          int32_t ThreadNum) {
  EDT::setMetadata(Fn, Builder);
}

void MainEDT::setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder) {
  TaskEDT::setMetadata(Fn, Builder, 0);
}

void SyncEDT::setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                          SetVector<EDT *> &Inputs, SetVector<EDT *> &Outputs,
                          bool SyncChilds, bool SyncDescendents,
                          int32_t ThreadNum) {
  TaskEDT::setMetadata(Fn, Builder, ThreadNum);
}

void ParallelEDT::setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                              int32_t ThreadNum, uint32_t NumThreads) {
  SetVector<EDT *> Inputs, Outputs;
  SyncEDT::setMetadata(Fn, Builder, Inputs, Outputs, true, true, ThreadNum);
}
