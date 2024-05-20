//===- ARTS.h - ARTS-Related stucts -----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "noelle/core/Task.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"

namespace arts {
/// Namespace for all ARTS related functionality.
using BlockSequence = SmallVector<BasicBlock *, 0>;
using namespace arcana::noelle;

/// Data Environment EDT
class EDT;

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
class EDT : public Task {
public:
  enum class Type { MAIN, TASK, LOOP, PARALLEL, WRAPPER, DONE, OTHER };

  EDT(Type Ty, FunctionType *TaskSignature, Module &M)
      : Task(TaskSignature, M), Env(this), Ty(Ty) {
    setName();
  }

  /// Analyzes the instructions in the EDT and removes the dead ones.
  void removeDeadInstructions();
  /// Others
  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);
  void cloneAndAddBasicBlocks(Function *F);
  void cloneAndAddBasicBlocks(BlockSequence &BBs);

  const std::string getName() { return ("edt_" + Twine(ID)).str(); }
  void setName() { F->setName("arts_" + getName()); }
  Instruction *getGuidAddr() { return GuidAddr; }
  Type getType() { return Ty; }
  EDTEnvironment &getEnv() { return Env; }
  BasicBlock *getBody() { return Body; }
  void setBody(BasicBlock *BB) { Body = BB; }
  std::unordered_map<Instruction *, std::unordered_set<Instruction *>> &
  getLiveOuts() {
    return liveOutClones;
  }
  std::unordered_map<Value *, Value *> &getLiveIns() {
    return liveInClones;
  }

  /// Attributes
  EDTEnvironment Env;
  Type Ty = Type::OTHER;
  BasicBlock *Body = nullptr;
  Instruction *GuidAddr = nullptr; // First instruction
  Instruction *CallInst = nullptr; // Last instruction
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &Edt) {
  OS << "EDT #" << Edt.getID() << "\n";
  OS << Edt.getEnv();
  OS << "Function: " << Edt.getTaskBody()->getName() << "\n";
  /// Live in instructions
  OS << "Live in instructions: \n";
  auto &LiveIns = Edt.getLiveIns();
  for (auto &LI : LiveIns) {
    OS << "  - " << *LI.first << "\n";
    OS << "    - " << *LI.second << "\n";
  }
  /// Live out instructions
  OS << "Live out instructions: \n";
  std::unordered_map<Instruction *, std::unordered_set<Instruction *>>
      &LiveOuts = Edt.getLiveOuts();
  for (auto &LO : LiveOuts) {
    OS << "  - " << *LO.first << "\n";
    for (auto &LOI : LO.second) {
      OS << "    - " << *LOI << "\n";
    }
  }
  return OS;
}

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
} // end namespace arts
#endif // LLVM_ARTS_H
