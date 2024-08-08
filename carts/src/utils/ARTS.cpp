#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Utils/Local.h"
#include <cassert>
#include <cstdint>
#include <string>

#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSTypes.h"

using namespace llvm;
using namespace std;
/// DEBUG
#define DEBUG_TYPE "arts"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

/// ------------------------------------------------------------------- ///
///                            ART TYPES                                ///
/// ------------------------------------------------------------------- ///
namespace arts::types {
const Twine toString(EDTType Ty) {
  switch (Ty) {
  case EDTType::Task:
    return "task";
  case EDTType::Main:
    return "main";
  case EDTType::Sync:
    return "sync";
  case EDTType::Parallel:
    return "parallel";
  default:
    break;
  }
  return "unknown";
}

const Twine toString(EDTArgType Ty) {
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
  if (Str == "task")
    return EDTType::Task;
  if (Str == "main")
    return EDTType::Main;
  if (Str == "sync")
    return EDTType::Sync;
  if (Str == "parallel")
    return EDTType::Parallel;
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

namespace arts {
/// ------------------------------------------------------------------- ///
///                            EDT DATABLOCK                            ///
/// ------------------------------------------------------------------- ///
EDTDataBlock::EDTDataBlock(EDTValue *V, Mode M, EDT *ContextEDT)
    : V(V), M(M), ContextEDT(ContextEDT) {}

/// Getters
EDTValue *EDTDataBlock::getValue() { return V; }
EDTDataBlock::Mode EDTDataBlock::getMode() { return M; }
EDT *EDTDataBlock::getContextEDT() { return ContextEDT; }
EDTDataBlock *EDTDataBlock::getParentDB() { return ParentDB; }
EDTDataBlockSet &EDTDataBlock::getChildrenDB() { return ChildrenDB; }
EDTDataBlock *EDTDataBlock::getDoneDB() { return DoneDB; };
EDTDataBlock *EDTDataBlock::getParentDoneDB() {
  if (ParentDB)
    return ParentDB->getDoneDB();
  return nullptr;
}
int32_t EDTDataBlock::getSlot() { return Slot; }
int32_t EDTDataBlock::getToSlot() {
  return getDoneDB() ? (getDoneDB()->getSlot())
                     : (getParentDoneDB()->getSlot());
}

/// Setters
void EDTDataBlock::setParentDB(EDTDataBlock *ParentDB) {
  this->ParentDB = ParentDB;
}
void EDTDataBlock::setSlot(int32_t Slot) { this->Slot = Slot; }
void EDTDataBlock::setDoneDB(EDTDataBlock *DoneDB) { this->DoneDB = DoneDB; }

/// Helpers
Type *EDTDataBlock::getType() {
  if (Ty)
    return Ty;
  /// Get the type of the value

  if (auto *Alloca = dyn_cast<AllocaInst>(V))
    Ty = Alloca->getAllocatedType();
  else if (auto *Load = dyn_cast<LoadInst>(V))
    Ty = Load->getType();
  /// If Ty is still null, return the type of the parent
  if (!Ty && ParentDB)
    Ty = ParentDB->getType();
  assert(Ty && "Type is null");
  return Ty;
}

bool EDTDataBlock::addChildDB(EDTDataBlock *ChildDB) {
  return ChildrenDB.insert(ChildDB);
}
/// ------------------------------------------------------------------- ///
///                          DATA ENVIRONMENT                           ///
/// ------------------------------------------------------------------- ///
EDTEnvironment::EDTEnvironment(EDT *E) : E(E) {}
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }
bool EDTEnvironment::isDepV(Value *V) { return DepV.count(V); }

/// ------------------------------------------------------------------- ///
///                                 EDT                                 ///
/// ------------------------------------------------------------------- ///
uint32_t EDT::Counter = 0;

EDT::EDT(EDTFunction *Fn) : Fn(Fn) {
  ID = Counter++;
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT #" << ID
                    << " for function: " << Fn->getName() << "\n");
  Env = new EDTEnvironment(this);

  /// Fill out the EDT Data Environment
  MDNode *MD = Fn->getMetadata(CARTS_MD);
  auto *ArgMD = dyn_cast<MDNode>(MD->getOperand(1).get());
  assert(ArgMD && "Arg Metadata Node is null");
  assert(ArgMD->getNumOperands() == Fn->arg_size() &&
         "Arg Metadata Node has invalid number of arguments");
  auto ArgMDItr = ArgMD->op_begin();
  for (auto &FnArg : Fn->args()) {
    Value *V = &FnArg;
    auto *ArgStr = dyn_cast<MDString>(ArgMDItr->get());
    assert(ArgStr && "Arg Metadata String is null");
    EDTArgType ArgTy = toEDTArgType(ArgStr->getString());
    switch (ArgTy) {
    case EDTArgType::Dep:
      LLVM_DEBUG(dbgs() << "   - DepV: " << *V << "\n");
      Env->insertDepV(V);
      break;
    case EDTArgType::Param:
      LLVM_DEBUG(dbgs() << "   - ParamV: " << *V << "\n");
      Env->insertParamV(V);
      break;
    default:
      llvm_unreachable("Unknown EDTArgType");
      break;
    }
    ArgMDItr++;
  }
}

EDT::~EDT() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying EDT #" << ID << "\n");
  delete Env;
}

/// Get the EDT from the function
EDT *EDT::get(EDTFunction *Fn) {
  if (!Fn->hasMetadata(CARTS_MD)) {
    LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
                      << " doesn't have CARTS Metadata\n");
    return nullptr;
  }
  LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
                    << " has CARTS Metadata\n");
  /// Get the metadata node
  MDNode *MD = Fn->getMetadata(CARTS_MD);
  assert(MD && "CARTS Metadata Node is null");
  assert(MD->getNumOperands() == 2 &&
         "CARTS Metadata Node is empty or incomplete");
  /// Get the EDT Type
  auto *TyMD = dyn_cast<MDString>(MD->getOperand(0).get());
  assert(TyMD && "EDT Type Metadata is null");
  EDTType Ty = toEDTType(TyMD->getString());

  switch (Ty) {
  case EDTType::Task:
    return new TaskEDT(Fn);
  case EDTType::Main:
    return new MainEDT(Fn);
  case EDTType::Sync:
    return new SyncEDT(Fn);
  case EDTType::Parallel:
    return new ParallelEDT(Fn);
  default:
    break;
  }
  return nullptr;
}

/// Data Environment
void EDT::insertValueToEnv(Value *Val) {
  /// Pointer is a depv, else, it is a paramv
  if (PointerType *PT = dyn_cast<PointerType>(Val->getType())) {
    Env->insertDepV(Val);
  } else
    Env->insertParamV(Val);
  /// TODO: Add extra info to OpenMP Frontend to have info about
  /// Data Sharing attributes
}

void EDT::insertValueToEnv(Value *Val, bool IsDepV) {
  /// Pointer is a depv, else, it is a paramv
  if (IsDepV) {
    Env->insertDepV(Val);
  } else
    Env->insertParamV(Val);
}

/// Getters
uint32_t EDT::getID() { return ID; }
EDTFunction *EDT::getFn() { return Fn; }
EDTEnvironment &EDT::getDataEnv() { return *Env; }
string EDT::getName() {
  return ("edt." + std::to_string(ID) + "." + toString(Ty)).str();
}
EDTTypeKind EDT::getTypeKind() const { return Kind; }
EDTType EDT::getTy() const { return Ty; }

/// Helpers
bool EDT::isAsync() { return !isa<SyncEDT>(this); }
bool EDT::isMain() { return isa<MainEDT>(this); }
bool EDT::isDep(Argument *Arg) { return Env->isDepV(Arg); }
bool EDT::isDep(uint32_t CallArgItr) { return isDep(Fn->getArg(CallArgItr)); }

/// Dependency Slot
uint32_t EDT::insertDepSlot(uint32_t CallArgItr) {
  uint32_t PrevSlot = DepSlot;
  DepSlotToArg[PrevSlot] = Fn->getArg(CallArgItr);
  DepSlot++;
  return PrevSlot;
}
uint32_t EDT::incDepSlot() { return DepSlot++; }
uint32_t EDT::getDepSlot() const { return DepSlot; }
Argument *EDT::getDepArg(uint32_t DepSlot) {
  auto Itr = DepSlotToArg.find(DepSlot);
  assert(Itr != DepSlotToArg.end() && "Dep Slot not found");
  return Itr->second;
}

/// What we learned after running the Attributor
void EDT::setCall(EDTCallBase *Call) { this->Call = Call; }
void EDT::setParent(EDT *Parent) { this->Parent = Parent; }
void EDT::setParentSync(EDT *ParentSync, bool SetDoneSync) {
  this->ParentSync = ParentSync;
  if (SetDoneSync)
    setDoneSync(ParentSync->getDoneSync());
}
void EDT::setDoneSync(EDT *DoneSync) {
  DoneSync->setIsDoneEDT(true);
  this->DoneSync = DoneSync;
}
void EDT::setNode(uint32_t Node) { this->Node = Node; }

EDTCallBase *EDT::getCall() { return Call; }
EDT *EDT::getParent() { return Parent; }
EDT *EDT::getParentSync() { return ParentSync; }
EDT *EDT::getDoneSync() { return DoneSync; }
uint32_t EDT::getNode() { return Node; }

/// Task EDT
TaskEDT::TaskEDT(EDTFunction *Fn) : EDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Task;
  Kind = EDTTypeKind::TK_TASK;
}

/// Main EDT
MainEDT::MainEDT(EDTFunction *Fn) : TaskEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Main EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Main;
  Kind = EDTTypeKind::TK_MAIN;
}

/// Sync EDT
SyncEDT::SyncEDT(EDTFunction *Fn) : TaskEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Sync EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Sync;
  Kind = EDTTypeKind::TK_SYNC;
}

/// Parallel EDT
ParallelEDT::ParallelEDT(EDTFunction *Fn) : SyncEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Parallel EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Parallel;
  Kind = EDTTypeKind::TK_PARALLEL;
  SyncChilds = true;
  SyncDescendants = true;
}

} // namespace arts