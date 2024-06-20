//===- ARTS.h - ARTS-Related stucts -----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <sys/types.h>

#include "carts/utils/ARTSMetadata.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace llvm;
using namespace std;

class EDT;

/// ------------------------------------------------------------------- ///
///                              EDTCache                               ///
/// The EDTCache provides information that is shared between the EDTs.
/// ------------------------------------------------------------------- ///
class EDTCache {
public:
  EDTCache(Module &M) : M(M){};
  ~EDTCache() = default;

  void insertEDT(Value *V, EDT *E);
  bool isValueInEDT(Value *V, EDT *E);
  SetVector<EDT *> getEDTs(Value *V) const;
  DenseMap<Value *, SetVector<EDT *>> &getValues() { return Values; }
  Module &getModule() { return M; }

private:
  Module &M;
  /// Values that may be modified by the EDTs (Calls)
  DenseMap<Value *, SetVector<EDT *>> Values;
};

/// ------------------------------------------------------------------- ///
///                          DATA ENVIRONMENT                           ///
/// Contains the list of parameters and dependencies required for the
/// EDT to run. It contains the following information:
/// - Paramc are the number of static parameters.
///   It corresponds to the number of first private variables.
/// - Depc is the number of dependencies required for the EDT to run.
///   It corresponds to the number of shared variables.
/// - Paramv are the static parameters that are copied into the EDT
///   closure.It corresponds to the private variables.
/// - Depv are the dependencies that are copied into the EDT closure.
///   It corresponds to the shared variables.
/// IMPORTANT: Firstprivate is live-in, shared is both live-in and
///            live-out, lastprivate is live-out
/// ------------------------------------------------------------------- ///
class EDTEnvironment {
public:
  EDTEnvironment(EDT *E);
  EDTEnvironment(EDT *E, SmallVector<EDTArgMetadata *, 4> &Args);

  /// Getters and setters
  void insertParamV(Value *V);
  void insertDepV(Value *V);
  uint32_t getParamC();
  uint32_t getDepC();

  /// Attributes
  EDT *E;
  /// These values correspond to the EDT Function arguments
  SetVector<Value *> ParamV;
  SetVector<Value *> DepV;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDTEnvironment &Env) {
  OS << "Data environment for EDT: \n";
  OS << "Number of ParamV: " << Env.ParamV.size() << "\n";
  for (auto &V : Env.ParamV) {
    OS << "  - " << *V << "\n";
  }
  OS << "Number of DepV: " << Env.DepV.size() << "\n";
  for (auto &V : Env.DepV) {
    OS << "  - " << *V << "\n";
  }
  return OS;
}

/// ------------------------------------------------------------------- ///
///                               EDT                                  ///
/// The EDT is the main abstraction used by ARTS to represent the tasks
/// in the program.
/// ------------------------------------------------------------------- ///
class EDT {
public:
  EDT(EDTCache &Cache, Function *Fn);
  EDT(EDTCache &Cache, EDTMetadata *MD);
  virtual ~EDT() = default;
  static EDT *get(EDTCache &Cache, EDTMetadata *MD);
  static uint32_t Counter;
  /// Data environment
  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);
  ///  Getters
  CallBase *getCall();
  Function *getFn();
  EDTEnvironment &getDataEnv();
  Twine getName();
  uint32_t getID();
  EDTType getTy() { return Ty; }
  EDTCache &getCache() { return Cache; }

  /// Setters
  void setCall(CallBase *Call);

  static bool classof(const EDTMetadata *M) { return true; }

  /// Override copy constructor to EDTMetadata
  EDT &operator=(const EDTMetadata &MD) {
    this->Fn = &MD.Fn;
    this->Ty = MD.Ty;
    return *this;
  }

protected:
  EDTCache &Cache;
  EDTEnvironment *Env;
  EDTType Ty = EDTType::Unknown;
  bool IsAsync = true;

private:
  CallBase *Call = nullptr;
  Function *Fn = nullptr;
  uint32_t ID;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &E) {
  OS << "- EDT for" << E.getName() << "\n";
  OS << "ID: " << E.getID() << "\n";
  OS << "Ty: " << toString(E.getTy()) << "\n";
  OS << E.getDataEnv();

  return OS;
}

class ParallelEDT : public EDT {
public:
  ParallelEDT(EDTCache &Cache, EDTMetadata *MD);
  ~ParallelEDT() = default;
  /// Getters
  uint32_t getNumThreads() { return NumThreads; }
  uint32_t getNumTasks() { return NumTasks; }

  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Parallel;
  }

  ParallelEDT &operator=(const ParallelEDTMetadata &MD) {
    this->NumThreads = MD.NumThreads;
    this->NumTasks = MD.NumTasks;
    return *this;
  }

private:
  uint32_t NumThreads;
  uint32_t NumTasks;
};

class TaskEDT : public EDT {
public:
  TaskEDT(EDTCache &Cache, EDTMetadata *MD);
  ~TaskEDT() = default;
  /// Getters
  uint32_t getThreadNum() { return ThreadNum; }

  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Task;
  }

  TaskEDT &operator=(const TaskEDTMetadata &MD) {
    this->ThreadNum = MD.ThreadNum;
    return *this;
  }

private:
  uint32_t ThreadNum;
};

class MainEDT : public EDT {
public:
  MainEDT(EDTCache &Cache, EDTMetadata *MD);
  ~MainEDT() = default;

  static bool classof(const EDTMetadata *M) {
    return M->getTy() == EDTType::Main;
  }

  MainEDT &operator=(const MainEDTMetadata &MD) { return *this; }

};

// inline raw_ostream &operator<<(raw_ostream &OS, EDTTask &Task) {
//   OS << "EDT #" << Task.getID() << "\n";
//   // OS << Task.getEnv();
//   OS << "Function: " << Task.getTaskBody()->getName() << "\n";
//   /// Live in instructions
//   OS << "Live in instructions: \n";
//   auto &LiveIns = Task.getLiveIns();
//   for (auto &LI : LiveIns) {
//     OS << "  - " << *LI.first << "\n";
//     OS << "    - " << *LI.second << "\n";
//   }
//   /// Live out instructions
//   OS << "Live out instructions: \n";
//   unordered_map<Instruction *, unordered_set<Instruction *>> &LiveOuts =
//       Task.getLiveOuts();
//   for (auto &LO : LiveOuts) {
//     OS << "  - " << *LO.first << "\n";
//     for (auto &LOI : LO.second) {
//       OS << "    - " << *LOI << "\n";
//     }
//   }
//   return OS;
// }
} // end namespace arts
#endif // LLVM_ARTS_H
