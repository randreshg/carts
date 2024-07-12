#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"
#include <cassert>
#include <cstdint>
#include <string>

#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSMetadata.h"
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
///                          DATA ENVIRONMENT                           ///
/// ------------------------------------------------------------------- ///
EDTEnvironment::EDTEnvironment(EDT *E) : E(E) {}
// EDTEnvironment::EDTEnvironment(EDT *E, SmallVector<EDTArgMetadata *, 4>
// &Args)
//     : E(E) {
//   /// Fill the environment based on the arguments metadata
//   for (auto *Arg : Args) {
//     Value *V = Arg->getVal();
//     if (Arg->getTy() == EDTArgType::Dep)
//       insertDepV(V);
//     else
//       insertParamV(V);
//   }
// }
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }
bool EDTEnvironment::isDepV(Value *V) { return DepV.count(V); }

/// ------------------------------------------------------------------- ///
///                                 EDT                                 ///
/// ------------------------------------------------------------------- ///
uint32_t EDT::Counter = 0;

EDT::EDT() {
  ID = Counter++;
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT #" << ID << "\n");
  /// Copy the metadata into the EDT
  // *this = *MD;
  /// Create the environment
  // Env = new EDTEnvironment(this, MD->Args);
}

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
  assert(MD->getNumOperands() > 0 && "CARTS Metadata Node is empty");
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

EDTEnvironment &EDT::getDataEnv() { return *Env; }
uint32_t EDT::getID() { return ID; }

bool EDT::isDep(uint32_t CallArgItr) {
  auto *Arg = Fn->getArg(CallArgItr);
  return Env->isDepV(Arg);
}

/// Task EDT
TaskEDT::TaskEDT(EDTFunction *Fn) : EDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Task;
}

/// Main EDT
MainEDT::MainEDT(EDTFunction *Fn) : TaskEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Main EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Main;
}

/// Sync EDT
SyncEDT::SyncEDT(EDTFunction *Fn) : TaskEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Sync EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Sync;
}

/// Parallel EDT
ParallelEDT::ParallelEDT(EDTFunction *Fn) : SyncEDT(Fn) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Parallel EDT for function: "
                    << Fn->getName() << "\n");
  Ty = EDTType::Parallel;
}

} // namespace arts