//===- ARTS.h - ARTS-Related stucts -----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/Casting.h"
#include <cstdint>
#include <sys/types.h>

// #include "carts/utils/ARTSMetadata.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace llvm;
using namespace std;

class EDT;
class EDTDataBlock;

using EDTValue = Value;
using EDTValueSet = SetVector<EDTValue *>;
using EDTCallBase = CallBase;
using EDTFunction = Function;
using EDTFunctionSet = SetVector<EDTFunction *>;
using EDTSet = SetVector<EDT *>;
using EDTDataBlockSet = SetVector<EDTDataBlock *>;

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
class EDTDataBlock {
public:
  enum Mode { ReadOnly, WriteOnly, ReadWrite };
  EDTDataBlock(EDTValue *V) : V(V) {}
  EDTDataBlock(EDTValue *V, Mode M) : V(V), M(M) {}
  ~EDTDataBlock() = default;

  EDTValue *getValue() { return V; }
  Mode getMode() { return M; }

private:
  /// Getters
  EDTValue *V = nullptr;
  Mode M = Mode::ReadWrite;
};

class EDTEnvironment {
public:
  EDTEnvironment(EDT *E);
  // EDTEnvironment(EDT *E, SmallVector<EDTArgMetadata *, 4> &Args);

  /// Getters and setters
  void insertParamV(EDTValue *V);
  void insertDepV(EDTValue *V);
  uint32_t getParamC();
  uint32_t getDepC();
  ///
  bool isDepV(EDTValue *V);

  /// Attributes
  EDT *E;
  /// These values correspond to the EDT Function arguments
  SetVector<EDTValue *> ParamV;
  SetVector<EDTValue *> DepV;
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
class SyncEDT;

class EDT {
public:
  EDT(EDTFunction *Fn);
  EDT();
  virtual ~EDT() = default;
  static EDT *get(EDTFunction *Fn);
  static uint32_t Counter;

  /// Data environment
  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);

  ///  Getters
  EDTEnvironment &getDataEnv();
  uint32_t getID();

  EDTType getTy() const { return Ty; }
  EDTFunction *getFn() { return Fn; }
  Twine getName() { return Fn->getName(); }

  bool isAsync() { return dyn_cast<SyncEDT>(this); }
  bool isDep(uint32_t CallArgItr);

  /// Static interface
  static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder);
  static bool classof(const EDT *M) { return true; }

protected:
  EDTFunction *Fn = nullptr;
  EDTEnvironment *Env = nullptr;
  EDTType Ty = EDTType::Unknown;

private:
  uint32_t ID;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &E) {
  OS << "- EDT #" << E.getID() << ": " << E.getName() << "\n";
  // OS << "Ty: " << toString(E.getTy()) << "\n";
  OS << E.getDataEnv();

  return OS;
}

class TaskEDT : public EDT {
public:
  TaskEDT(EDTFunction *Fn);
  ~TaskEDT() = default;

  /// Getters
  int32_t getThreadNum() { return ThreadNum; }

  /// Static interface
  static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                          int32_t ThreadNum = -1);
  static bool classof(const TaskEDT *M) { return M->getTy() == EDTType::Task; }

private:
  int32_t ThreadNum = -1;
};

class MainEDT : public TaskEDT {
public:
  MainEDT(EDTFunction *Fn);
  ~MainEDT() = default;

  static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder);
  static bool classof(const EDT *M) { return M->getTy() == EDTType::Main; }

  MainEDT &operator=(const MainEDT &MD) { return *this; }
};

class SyncEDT : public TaskEDT {
public:
  SyncEDT(EDTFunction *Fn);
  ~SyncEDT() = default;

  /// Getters
  bool mustSyncChilds() const { return SyncChilds; }
  bool mustSyncDescendents() const { return SyncDescendents; }

  static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                          SetVector<EDT *> &Inputs, SetVector<EDT *> &Outputs,
                          bool SyncChilds = true, bool SyncDescendents = false,
                          int32_t ThreadNum = -1);
  static bool classof(const EDT *M) { return M->getTy() == EDTType::Sync; }

protected:
  bool SyncChilds = true;
  bool SyncDescendents = true;
  SetVector<EDT *> Inputs;
  SetVector<EDT *> Outputs;
};

class ParallelEDT : public SyncEDT {
public:
  ParallelEDT(EDTFunction *Fn);
  ~ParallelEDT() = default;

  uint32_t getNumThreads() const { return NumThreads; }

  static void setMetadata(EDTFunction *Fn, EDTIRBuilder &Builder,
                          int32_t ThreadNum = -1, uint32_t NumThreads = 1);

protected:
  uint32_t NumThreads = 1;
};

} // end namespace arts
#endif // LLVM_ARTS_H
