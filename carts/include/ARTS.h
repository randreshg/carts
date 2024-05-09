//===- ARTSConstants.h - OpenMP related constants and helpers ------ C++
//-*-===//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file defines constants and helpers used when dealing with OpenMP.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "noelle/core/Noelle.hpp"
#include "noelle/core/Task.hpp"
#include "llvm/IR/InstrTypes.h"

namespace arts {
/// Namespace for all ARTS related functionality.
using BlockSequence = SmallVector<BasicBlock *, 0>;
using namespace arcana::noelle;

/// Data Environment EDT
class EDT;
class EDTEnvironment {
public:
  EDTEnvironment(EDT *E) : E(E) {}

  void insertParamV(Value *V);
  void insertDepV(Value *V);

  EDT *E;
  /// Paramc are the number of static parameters.
  /// It corresponds to the number of first private variables.
  uint64_t ParamC = 0;
  /// Depc is the number of dependencies required for the EDT to run.
  /// It corresponds to the number of shared variables.
  uint64_t DepC = 0;
  /// Paramv are the static parameters that are copied into the EDT closure.
  /// It corresponds to the private variables.
  SetVector<Value *> ParamV;
  /// Depv are the dependencies that are copied into the EDT closure.
  /// It corresponds to the shared variables.
  SetVector<Value *> DepV;

  /// IMPORTANT: Firstprivate is live-in, shared is both live-in and live-out,
  /// lastprivate is live-out
};

inline raw_ostream &operator<<(raw_ostream &OS, EDTEnvironment &Env) {
  OS << "Data environment for EDT: \n";
  OS << "Number of ParamV: " << Env.ParamV.size() << "\n";
  OS << "Number of DepV: " << Env.DepV.size() << "\n";
  return OS;
}

/// EDT
class EDT : public Task {
public:
  enum class Type {
    MAIN,
    TASK,
    LOOP,
    PARALLEL,
    WRAPPER,
    OTHER,
  };

  EDT(Type Ty, FunctionType *TaskSignature, Module &M,
      const std::string &EDTName)
      : Task(TaskSignature, M, EDTName), Env(this), Ty(Ty) {}

  void insertValueToEnv(Value *Val);
  void cloneAndAddBasicBlocks(Function *F);
  void setF(Function *F) { this->F = F; }
  Type getType() { return Ty; }
  EDTEnvironment &getEnv() { return Env; }

  /// Attributes
  EDTEnvironment Env;
  Type Ty = Type::OTHER;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &Edt) {
  OS << "EDT #" << Edt.getID() << "\n";
  OS << Edt.getEnv() << "\n";
  OS << "Function: " << *Edt.getTaskBody() << "\n";
  return OS;
}

/// ----------------------------------------------------- ///
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
