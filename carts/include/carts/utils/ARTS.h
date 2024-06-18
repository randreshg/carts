//===- ARTS.h - ARTS-Related stucts -----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "llvm/ADT/SetVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include <sys/types.h>

#include "carts/utils/ARTSMetadata.h"


namespace arts {
using namespace llvm;
using namespace std;

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
class EDT;
class EDTCache {
public:
  EDTCache(Module &M): M(M) {};
  ~EDTCache() = default;


  void insertEDT(Value *V, EDT *E);
  bool isValueInEDT(Value *V, EDT *E);
  SetVector<EDT *> getEDTs(Value *V) const;
  Module &getModule() { return M; }

private:
  Module &M;
  DenseMap<Value *, SetVector<EDT *>> Values;
};

class EDTEnvironment {
public:
  EDTEnvironment(EDT *E) : E(E) {}

  void insertParamV(Value *V);
  void insertDepV(Value *V);
  uint32_t getParamC();
  uint32_t getDepC();

  /// Attributes
  EDT *E;
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
  EDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call);
  virtual ~EDT() = default;
  /// Data environment
  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);
  ///  Getters
  CallBase *getCall();
  Function *getFn();
  EDTEnvironment &getDataEnv();
  virtual EDTMetadata *getMD();
  Twine getName();
  uint32_t getID();

protected:
  EDTCache &Cache;
  EDTMetadata *MD;
  EDTEnvironment Env;

private:
  CallBase *Call = nullptr;
  static uint32_t ID;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &E) {
  OS << "- EDT for" << E.getName() << "\n";
  OS << E.getDataEnv();

  return OS;
}

class ParallelEDT : public EDT {
public:
  ParallelEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call);
  ~ParallelEDT() = default;
  ParallelEDTMetadata *getMD() override;
};

class TaskEDT : public EDT {
public:
  TaskEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call);
  ~TaskEDT() = default;
  TaskEDTMetadata *getMD() override;
};

class MainEDT : public EDT {
public:
  MainEDT(EDTCache &Cache, EDTMetadata *MD, CallBase *Call);
  ~MainEDT() = default;
  MainEDTMetadata *getMD() override;
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
