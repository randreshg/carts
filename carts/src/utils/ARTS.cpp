#include "llvm/ADT/StringRef.h"
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
  if (Str.equals(ARTS_EDT_TASK))
    return EDTType::Task;
  if (Str.equals(ARTS_EDT_MAIN))
    return EDTType::Main;
  if (Str.equals(ARTS_EDT_SYNC))
    return EDTType::Sync;
  if (Str.equals(ARTS_EDT_PARALLEL))
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
///                               DATA BLOCK                            ///
/// ------------------------------------------------------------------- ///
DataBlock::DataBlock(EDTValue *V, Mode M, EDT *ContextEDT)
    : V(V), M(M), ContextEDT(ContextEDT) {}

/// Getters
EDTValue *DataBlock::getValue() { return V; }
DataBlock::Mode DataBlock::getMode() { return M; }
EDT *DataBlock::getContextEDT() { return ContextEDT; }
DataBlock *DataBlock::getParent() { return Parent; }
DataBlock *DataBlock::getParentSync() { return ParentSync; }
DataBlock *DataBlock::getMainParent() {
  if (MainParent)
    return MainParent;

  /// If I don't have a parent, return nullptr
  if (!Parent)
    return nullptr;

  /// If I have a parent, get its main parent. If it has one,
  /// return it
  if (DataBlock *ParentMain = Parent->getMainParent())
    MainParent = ParentMain;
  /// Otherwise, return my parent
  else
    MainParent = Parent;
  return MainParent;
}
uint32_t DataBlock::getSlot() { return Slot; }
DataBlockSet &DataBlock::getChildDBs() { return ChildDBs; }
string DataBlock::getName() {
  if (Name.empty()) {
    string ValueName = V->getName().str();
    if (!ValueName.empty())
      Name = "db." + ValueName;
    else if (Parent)
      Name = Parent->getName();
    else
      Name = "db." + std::to_string(Slot);
    return Name;
  }
  return Name;
  // return "db." + std::to_string(Slot);
}

/// Insert
bool DataBlock::addChildDB(DataBlock *ChildDB) {
  bool Inserted = ChildDBs.insert(ChildDB);
  if (Inserted) {
    assert(!ChildDB->getParent() && "Parent was already set");
    ChildDB->setParent(this);
    return true;
  }
  return false;
}

/// Setters
void DataBlock::setParent(DataBlock *Parent) { this->Parent = Parent; }
void DataBlock::setParentSync(DataBlock *ParentSync) {
  this->ParentSync = ParentSync;
}
void DataBlock::setSlot(uint32_t Slot) { this->Slot = Slot; }

/// Helpers
Type *DataBlock::getType() {
  if (Ty)
    return Ty;
  /// Get the type of the value
  if (AllocaInst *Alloca = dyn_cast<AllocaInst>(V))
    Ty = Alloca->getAllocatedType();
  else if (LoadInst *Load = dyn_cast<LoadInst>(V))
    Ty = Load->getType();
  /// If Ty is still null, return the type of the parent
  if (!Ty && Parent)
    Ty = Parent->getType();
  assert(Ty && "Type is null");
  return Ty;
}

// bool DataBlock::addChildDB(DataBlock *ChildDB) {
//   return ChildrenDB.insert(ChildDB);
// }
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

  /// Fill out the EDT Data Environment based on the function arguments
  // for (Argument &Arg : Fn->args()) {
  //   /// Analyze function argument annotations
  //   insertValueToEnv(&Arg);
  // }
}

EDT::~EDT() {
  LLVM_DEBUG(dbgs() << TAG << "Destroying EDT #" << ID << "\n");
  delete Env;
}

/// Get the EDT from the function
EDT *EDT::get(EDTFunction *Fn, string Annotation) {
  StringRef EDTTy;
  SmallVector<StringRef, 4> SymInputs, SymOutputs;

  /// Lambda function to parse the annotation
  auto parseAnnotation = [&](StringRef Annotation) {
    /// Type of annotation: arts.parallel, arts.task, arts.single...
    size_t Pos = Annotation.find(' ');
    if (Pos == StringRef::npos)
      return;

    EDTTy = Annotation.substr(0, Pos);
    LLVM_DEBUG(dbgs() << TAG << "  - Type: " << EDTTy << "\n");

    /// Remove the type from the annotation
    Annotation = Annotation.substr(Pos + 1);

    auto SplitAnnotation = [](StringRef Annot) {
      SmallVector<StringRef, 4> Tokens;
      Annot.split(Tokens, ',');
      return Tokens;
    };

    /// Parse the dependencies
    if (Annotation.starts_with("deps(in: ")) {
      Annotation = Annotation.substr(9);

      Pos = Annotation.find(')');
      if (Pos == StringRef::npos)
        return;

      SymInputs = SplitAnnotation(Annotation.substr(0, Pos));
      LLVM_DEBUG(dbgs() << TAG << "  - Input dependencies:\n");
      for (StringRef Input : SymInputs) {
        LLVM_DEBUG(dbgs() << TAG << "    - " << Input << "\n");
      }

      /// Remove the dependencies from the annotation
      Annotation = Annotation.substr(Pos + 2);
    }

    if (Annotation.starts_with("deps(out: ")) {
      Annotation = Annotation.substr(10);

      Pos = Annotation.find(')');
      if (Pos == StringRef::npos)
        return;

      SymOutputs = SplitAnnotation(Annotation.substr(0, Pos));
      LLVM_DEBUG(dbgs() << TAG << "  - Output dependencies:\n");
      for (StringRef Output : SymOutputs) {
        LLVM_DEBUG(dbgs() << TAG << "    - " << Output << "\n");
      }
    }
  };

  parseAnnotation(Annotation);
  if (EDTTy.empty()) {
    LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
                      << " doesn't have CARTS Metadata\n");
    return nullptr;
  }

  LLVM_DEBUG(dbgs() << TAG << "Function: " << Fn->getName()
                    << " has ARTS Metadata\n");
  switch (toEDTType(EDTTy)) {
  case EDTType::Task:
    return new TaskEDT(Fn, SymInputs, SymOutputs);
  case EDTType::Main:
    return new MainEDT(Fn);
  case EDTType::Sync:
    return new SyncEDT(Fn);
  case EDTType::Parallel:
    return new ParallelEDT(Fn);
  default:
    LLVM_DEBUG(dbgs() << TAG << "Unknown EDTType: " << EDTTy << "\n");
    return nullptr;
  }
}

/// Data Environment
void EDT::insertValueToEnv(Value *Val) {
  /// Pointer is a depv, else, it is a paramv
  if (PointerType *PT = dyn_cast<PointerType>(Val->getType())) {
    Env->insertDepV(Val);
  } else
    Env->insertParamV(Val);
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
string EDT::getTag() { return ("edt_" + std::to_string(ID)); }
string EDT::getName() { return (getTag() + "." + toString(Ty)).str(); }
EDTTypeKind EDT::getTypeKind() const { return Kind; }
EDTType EDT::getTy() const { return Ty; }

/// Helpers
bool EDT::isSync() { return isa<SyncEDT>(this); }
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

TaskEDT::TaskEDT(EDTFunction *Fn, SmallVector<StringRef, 4> Inputs,
                 SmallVector<StringRef, 4> Outputs)
    : EDT(Fn), SymInputs(Inputs), SymOutputs(Outputs) {
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