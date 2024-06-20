
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
EDTArgMetadata::EDTArgMetadata(Value *V) : V(V) {}
EDTArgMetadata::~EDTArgMetadata() {}

string EDTArgMetadata::getName(EDTArgType Ty) {
  switch (Ty) {
  case EDTArgType::Dep:
    return EDTDepArgMetadata::getName();
  case EDTArgType::Param:
    return EDTParamArgMetadata::getName();
  default:
    return "";
  }
}

/// EDTDepsArgMetadata
EDTDepArgMetadata::EDTDepArgMetadata(Value *V) : EDTArgMetadata(V) {
  Ty = EDTArgType::Dep;
}
EDTDepArgMetadata::~EDTDepArgMetadata() {}

string EDTDepArgMetadata::getName() {
  return (toString(EDTArgType::Dep)).str();
}

/// EDTParamArgMetadata
EDTParamArgMetadata::EDTParamArgMetadata(Value *V) : EDTArgMetadata(V) {
  Ty = EDTArgType::Param;
}

EDTParamArgMetadata::~EDTParamArgMetadata() {}

string EDTParamArgMetadata::getName() {
  return (toString(EDTArgType::Param)).str();
}

/// ------------------------------------------------------------------- ///
///                             EDT METADATA                            ///
/// ------------------------------------------------------------------- ///
EDTMetadata::EDTMetadata(Function &Fn) : Fn(Fn) {}
EDTMetadata::~EDTMetadata() {
  for (auto *Arg : Args)
    delete Arg;
}

string EDTMetadata::getName(EDTType Ty) {
  switch (Ty) {
  case EDTType::Parallel:
    return ParallelEDTMetadata::getName();
  case EDTType::Task:
    return TaskEDTMetadata::getName();
  case EDTType::Main:
    return MainEDTMetadata::getName();
  default:
    return "";
  }
}

EDTMetadata *EDTMetadata::getMetadata(Function &Fn) {
  /// Check if the function has carts metadata
  if (!Fn.hasMetadata(CARTS_MD)) {
    LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn.getName()
                      << " doesn't have CARTS Metadata\n");
    return nullptr;
  }
  LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn.getName()
                    << " has CARTS Metadata\n");
  /// Get the metadata node
  MDNode *MD = Fn.getMetadata(CARTS_MD);
  assert(MD && "CARTS Metadata Node is null");
  assert(MD->getNumOperands() > 0 && "CARTS Metadata Node is empty");
  /// Get the EDT Type
  auto *TyMD = dyn_cast<MDString>(MD->getOperand(0).get());
  assert(TyMD && "EDT Type Metadata is null");
  /// Get the EDT Metadata
  EDTMetadata *EDTMD;
  EDTType Ty = toEDTType(TyMD->getString());
  switch (Ty) {
  case EDTType::Parallel:
    EDTMD = new ParallelEDTMetadata(Fn);
    break;
  case EDTType::Task:
    EDTMD = new TaskEDTMetadata(Fn);
    break;
  case EDTType::Main:
    return new MainEDTMetadata(Fn);
    break;
  default:
    return nullptr;
  }
  assert(EDTMD && "EDT Metadata is null");
  /// Get the EDT Args metadata
  auto *ArgMD = dyn_cast<MDNode>(MD->getOperand(1).get());
  assert(ArgMD && "Arg Metadata Node is null");
  /// Function argument itr
  assert(ArgMD->getNumOperands() == Fn.arg_size() &&
         "Arg Metadata Node has invalid number of arguments");
  auto ArgMDItr = ArgMD->op_begin();
  for (auto &FnArg : Fn.args()) {
    Value *V = &FnArg;
    auto *ArgStr = dyn_cast<MDString>(ArgMDItr->get());
    assert(ArgStr && "Arg Metadata String is null");
    EDTArgType ArgTy = toEDTArgType(ArgStr->getString());
    switch (ArgTy) {
    case EDTArgType::Dep:
      EDTMD->insertArg(new EDTDepArgMetadata(V));
      break;
    case EDTArgType::Param:
      EDTMD->insertArg(new EDTParamArgMetadata(V));
      break;
    default:
      break;
    }
    ArgMDItr++;
  }

  return EDTMD;
}

void EDTMetadata::setMetadata(Function &Fn, EDTIRBuilder &Builder) {
  LLVMContext &Ctx = Fn.getContext();
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
  Fn.setMetadata(CARTS_MD, MDNode::get(Ctx, EDTMDs));
}

/// ParallelEDTMetadata
ParallelEDTMetadata::ParallelEDTMetadata(Function &Fn) : EDTMetadata(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Parallel EDT Metadata\n");
  Ty = EDTType::Parallel;
}

string ParallelEDTMetadata::getName() {
  return (toString(EDTType::Parallel)).str();
}

/// TaskEDTMetadata
TaskEDTMetadata::TaskEDTMetadata(Function &Fn) : EDTMetadata(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task EDT Metadata\n");
  Ty = EDTType::Task;
}

string TaskEDTMetadata::getName() { return (toString(EDTType::Task)).str(); }

/// MainEDTMetadata
MainEDTMetadata::MainEDTMetadata(Function &Fn) : EDTMetadata(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Main EDT Metadata\n");
  Ty = EDTType::Main;
}

string MainEDTMetadata::getName() { return (toString(EDTType::Main)).str(); }
