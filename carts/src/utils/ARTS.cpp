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

namespace arts {
/// ------------------------------------------------------------------- ///
///                          DATA ENVIRONMENT                           ///
/// ------------------------------------------------------------------- ///
EDTEnvironment::EDTEnvironment(EDT *E) : E(E) {}
EDTEnvironment::EDTEnvironment(EDT *E, SmallVector<EDTArgMetadata *, 4> &Args)
    : E(E) {
  /// Fill the environment based on the arguments metadata
  for (auto *Arg : Args) {
    Value *V = Arg->getV();
    if (Arg->getTy() == EDTArgType::Dep)
      insertDepV(V);
    else
      insertParamV(V);
  }
}
void EDTEnvironment::insertParamV(Value *V) { ParamV.insert(V); }
void EDTEnvironment::insertDepV(Value *V) { DepV.insert(V); }
uint32_t EDTEnvironment::getParamC() { return ParamV.size(); }
uint32_t EDTEnvironment::getDepC() { return DepV.size(); }

/// ------------------------------------------------------------------- ///
///                                 EDT                                 ///
/// ------------------------------------------------------------------- ///
uint32_t EDT::Counter = 0;

EDT::EDT(EDTMetadata *MD) {
  ID = Counter++;
  LLVM_DEBUG(dbgs() << TAG << "Creating EDT #" << ID << "\n");
  /// Copy the metadata into the EDT
  *this = *MD;
  /// Create the environment
  Env = new EDTEnvironment(this, MD->Args);
}

EDT *EDT::get(EDTMetadata *MD) {
  switch (MD->getTy()) {
  case EDTType::Parallel:
    return new ParallelEDT(MD);
  case EDTType::Task:
    return new TaskEDT(MD);
  case EDTType::Main:
    return new MainEDT(MD);
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

EDTCallBase EDT::getCall() { return Call; }
EDTFunction EDT::getFn() { return Fn; }
EDTEnvironment &EDT::getDataEnv() { return *Env; }
Twine EDT::getName() { return Fn->getName(); }
uint32_t EDT::getID() { return ID; }
void EDT::setCall(CallBase *Call) { 
  assert((Call && !(this->Call)) && "Invalid Call");
  this->Call = Call;
  /// Update cache
  // for(auto &Arg : Call->args())
  //   Cache.insertEDT(Arg, this);
}

/// Parallel EDT
ParallelEDT::ParallelEDT(EDTMetadata *MD) : EDT(MD) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Parallel EDT for function: "
                    << getFn()->getName() << "\n");
  Ty = EDTType::Parallel;
  IsAsync = false;
  assert(MD->getTy() == Ty && "Invalid EDT Metadata Type");
  /// Copy the metadata into the Parallel EDT
  auto *PMD = cast<ParallelEDTMetadata>(MD);
  *this = *PMD;
}

/// Task EDT
TaskEDT::TaskEDT(EDTMetadata *MD) : EDT(MD) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task EDT for function: "
                    << getFn()->getName() << "\n");
  Ty = EDTType::Task;
  assert(MD->getTy() == Ty && "Invalid EDT Metadata Type");
  /// Copy the metadata into the Task EDT
  auto *TMD = cast<TaskEDTMetadata>(MD);
  *this = *TMD;
}

/// Main EDT
MainEDT::MainEDT(EDTMetadata *MD) : EDT(MD) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Main EDT for function: "
                    << getFn()->getName() << "\n");
  Ty = EDTType::Main;
  assert(MD->getTy() == Ty && "Invalid EDT Metadata Type");
  /// Copy the metadata into the Main EDT
  auto *MMD = cast<MainEDTMetadata>(MD);
  *this = *MMD;
}

} // namespace arts