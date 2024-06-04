//===- ARTS.h - ARTS-Related stucts -----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "noelle/core/Noelle.hpp"
#include "noelle/core/Task.hpp"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

namespace arts {
/// Namespace for all ARTS related functionality.
using BlockSequence = SmallVector<BasicBlock *, 0>;
using namespace arcana;
using namespace std;
/// ------------------------------------------------------------------- ///
///                            ART TYPES                                ///
/// It contains the types used by the ARTS runtime library.
/// ------------------------------------------------------------------- ///
namespace types {
/// IDs for all arts runtime library (RTL) functions.
enum class RuntimeFunction {
#define ARTS_RTL(Enum, ...) Enum,
#include "ARTSKinds.def"
};

#define ARTS_RTL(Enum, ...)                                                    \
  constexpr auto Enum = arts::types::RuntimeFunction::Enum;
#include "ARTSKinds.def"

} // end namespace types

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
/// IMPORTANT: Firstprivate is live-in, shared is both live-in and live-out,
/// lastprivate is live-out
/// ------------------------------------------------------------------- ///
class EDT;
class EDTCache {
public:
  EDTCache(Module &M, noelle::Noelle &NM);
  ~EDTCache();

  void insertEDT(Value *V, EDT *E);
  bool isValueInEDT(Value *V, EDT *E);
  SetVector<EDT *> getEDTs(Value *V) const;
  noelle::Noelle &getNoelle() { return NM; }
  Module &getModule() { return M; }

private:
  Module &M;
  noelle::Noelle &NM;
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
///                          EDT INTERFACE                              ///
/// The EDT is the main abstraction used by ARTS to represent the tasks
/// in the program.
/// ------------------------------------------------------------------- ///
class EDTTask : public noelle::Task {
public:
  EDTTask(FunctionType *TaskSignature, Module &M) : Task(TaskSignature, M){};
  EDTTask(FunctionType *TaskSignature, Module &M, const string &TaskName)
      : Task(TaskSignature, M, TaskName) {}

  void removeDeadInstructions();
  void cloneAndAddBasicBlocks(Function *F);
  void cloneAndAddBasicBlocks(BlockSequence &BBs);
  unordered_map<Instruction *, unordered_set<Instruction *>> &getLiveOuts() {
    return liveOutClones;
  }
  unordered_map<Value *, Value *> &getLiveIns() { return liveInClones; }

  /// Getters and setters
  Instruction *getGuidAddr() { return GuidAddr; }
  BasicBlock *getBody() { return Body; }
  void setBody(BasicBlock *BB) { Body = BB; }
  const string getName() { return ("edt_" + Twine(ID)).str(); }
  void setName() { F->setName("arts_" + getName()); }

private:
  /// Attributes
  BasicBlock *Body = nullptr;
  /// Call to GuidAddr allocation
  Instruction *GuidAddr = nullptr;
  /// Call to EDTCreate function
  Instruction *CallBase = nullptr;
};

class EDT {
public:
  EDT(EDTCache &Cache) : Cache(Cache), Env(this){};

  virtual ~EDT() = default;

  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);

  CallBase *getCall() { return OutlinedFnCall; }
  Function *getOutlinedFn() { return OutlinedFn; }
  EDTEnvironment &getDataEnv() { return Env; }
  EDTTask *getTask() { return Task; }
  void setTask(EDTTask *T) { Task = T; }
  string getName() { return Task->getName(); }
  StringRef getOutlinedFnName() {
    if (!OutlinedFn)
      return "MainFunction";
    return OutlinedFn->getName();
  }

  virtual void createTask() = 0;
  virtual void setDataEnv(CallBase *CB) = 0;

protected:
  EDTCache &Cache;
  EDTEnvironment Env;
  EDTTask *Task;
  /// Outlined Function Info
  CallBase *OutlinedFnCall = nullptr;
  Function *OutlinedFn = nullptr;
  DenseMap<Value *, Value *> RewiringMap;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &E) {
  OS << "- EDT for" << E.getOutlinedFnName() << "\n";
  OS << E.getDataEnv();

  return OS;
}

class ParallelEDT : public EDT {
public:
  ParallelEDT(EDTCache &Cache) : EDT(Cache) {}
  ParallelEDT(EDTCache &Cache, CallBase *OutlinedFnCall);
  ~ParallelEDT() = default;

  void createTask() override;
  void setDataEnv(CallBase *CB) override;
};

class ParallelDoneEDT : public EDT {
public:
  ParallelDoneEDT(EDTCache &Cache) : EDT(Cache) {}
  ParallelDoneEDT(EDTCache &Cache, CallBase *OutlinedFnCall);
  ~ParallelDoneEDT() = default;

  void createTask() override;
  void setDataEnv(CallBase *CB) override;
  void createOutlinedFn(CallBase *CB);
};

class TaskEDT : public EDT {
public:
  TaskEDT(EDTCache &Cache) : EDT(Cache) {}
  TaskEDT(EDTCache &Cache, CallBase *OutlinedFnCall);
  ~TaskEDT() = default;

  void createTask() override;
  void setDataEnv(CallBase *CB) override;
};

class MainEDT : public EDT {
public:
  MainEDT(EDTCache &Cache) : EDT(Cache) {}
  ~MainEDT() = default;

  void createTask() override;
  void setDataEnv(CallBase *CB) override;
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
