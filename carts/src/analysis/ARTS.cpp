#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/Local.h"
#include <cstdint>

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSTypes.h"
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
EDT::EDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call)
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
Function *EDT::getFn() { return &MD->getFunction(); }
EDTEnvironment &EDT::getDataEnv() { return Env; }
EDTMetadata *EDT::getMD() { return MD; }
Twine EDT::getName() { return getFn()->getName(); }
uint32_t EDT::getID() { return ID; }

/// Parallel EDT
ParallelEDT::ParallelEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call)
    : EDT(Cache, MD, Call) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Parallel EDT for function: "
                    << getFn()->getName() << "\n");
}

ParallelEDTMetadata *ParallelEDT::getMD() {
  return dyn_cast<ParallelEDTMetadata>(MD);
}

/// Task EDT
TaskEDT::TaskEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call)
    : EDT(Cache, MD, Call) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Task EDT for function: "
                    << getFn()->getName() << "\n");
}

TaskEDTMetadata *TaskEDT::getMD() { return dyn_cast<TaskEDTMetadata>(MD); }

/// Main EDT
MainEDT::MainEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call)
    : EDT(Cache, MD, Call) {
  LLVM_DEBUG(dbgs() << TAG << "Creating Main EDT for function: "
                    << getFn()->getName() << "\n");
}

MainEDTMetadata *MainEDT::getMD() { return dyn_cast<MainEDTMetadata>(MD); }
