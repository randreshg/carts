//============================================================================//
//                                   ARTS.h                                   //
//============================================================================//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "llvm/ADT/SetVector.h"
// #include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <sys/types.h>

// #include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace types;

/// ------------------------------------------------------------------- ///
///                           DATABLOCK (DB)                            ///
/// It represents the data that are required for the EDT to run.
/// It contains the following information:
/// - V is the value that is being passed to the EDT
/// - M is the mode of the DB (ReadOnly, WriteOnly, ReadWrite)
/// - ContextEDT is the EDT that is using DataBlock
/// - Parent is the parent DB
/// - ChildrenDB are the children DBs that are using the DB
/// - Done is the DB that is used to signal the completion of the DB
/// - Slot is the slot number of the DB
/// - ToSlot is the slot number of the DB that is used to signal the
///   completion of the DB
/// ------------------------------------------------------------------- ///
class DataBlock {
public:
  enum Mode { ReadOnly, WriteOnly, ReadWrite };
  DataBlock(EDTValue *V, Mode M, EDT *ContextEDT);
  ~DataBlock() = default;

  /// Getters
  EDTValue *getValue();
  Type *getType();
  Mode getMode();
  EDT *getContextEDT();
  uint32_t getSlot();
  DataBlock *getParent();
  DataBlock *getParentSync();
  DataBlock *getMainParent();
  DataBlockSet &getChildDBs();
  string getName();

  /// Setters
  void setParent(DataBlock *Parent);
  void setParentSync(DataBlock *ParentSync);
  void setSlot(uint32_t Slot);

  /// Helpers
  bool addChildDB(DataBlock *ChildDB);

private:
  EDTValue *V = nullptr;
  Type *Ty = nullptr;
  Mode M = Mode::ReadWrite;
  EDT *ContextEDT = nullptr;
  uint32_t Slot;
  DataBlock *Parent = nullptr;
  DataBlock *ParentSync = nullptr;
  DataBlock *MainParent = nullptr;
  DataBlock *Done = nullptr;
  DataBlockSet ChildDBs;
  string Name;
};

inline raw_ostream &operator<<(raw_ostream &OS, DataBlock &DB) {
  OS << *DB.getValue() << " / ";
  switch (DB.getMode()) {
  case DataBlock::Mode::ReadOnly:
    OS << "ReadOnly";
    break;
  case DataBlock::Mode::WriteOnly:
    OS << "WriteOnly";
    break;
  case DataBlock::Mode::ReadWrite:
    OS << "ReadWrite";
    break;
  }
  return OS;
}

/// ------------------------------------------------------------------- ///
///                           EDT ENVIRONMENT                           ///
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

  /// Getters and setters
  void insertParamV(EDTValue *V);
  void insertDepV(EDTValue *V);
  uint32_t getParamC();
  uint32_t getDepC();
  /// Other
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
class EDT {
public:
  EDT(EDTFunction *Fn);
  virtual ~EDT();

  /// Static interface
  static uint32_t Counter;
  static EDT *get(EDTFunction *Fn, string Annotation);
  static bool classof(const EDT *E) { return true; }

  /// Data Environment
  void insertValueToEnv(Value *Val);
  void insertValueToEnv(Value *Val, bool IsDepV);

  ///  Getters
  uint32_t getID();
  EDTFunction *getFn();
  EDTEnvironment &getDataEnv();
  string getName();
  string getTag();
  EDTTypeKind getTypeKind() const;
  EDTType getTy() const;

  /// Dependency Slot
  uint32_t insertDepSlot(uint32_t CallArgItr);
  uint32_t incDepSlot();
  uint32_t getDepSlot() const;
  Argument *getDepArg(uint32_t DepSlot);

  /// Helpers
  bool isSync();
  bool isAsync();
  bool isMain();
  bool isDep(Argument *Arg);
  bool isDep(uint32_t CallArgItr);
  Argument *getArg(uint32_t DepSlot);

protected:
  EDTFunction *Fn = nullptr;
  EDTEnvironment *Env = nullptr;
  EDTType Ty = EDTType::Unknown;
  EDTTypeKind Kind;

private:
  uint32_t ID;
  uint32_t DepSlot = 0;
  DenseMap<uint32_t, Argument *> DepSlotToArg;

  /// What we learned after running the Attributor
public:
  void setCall(EDTCallBase *Call);
  void setParent(EDT *Parent);
  void setDoneSync(EDT *DoneSync);
  void setParentSync(EDT *ParentSync, bool SetDoneSync = true);
  void setNode(uint32_t Node);
  void setIsDoneEDT(bool IsDoneEDT) { this->IsDoneEDT = IsDoneEDT; }

  EDTCallBase *getCall();
  EDT *getParent();
  EDT *getParentSync();
  EDT *getDoneSync();
  uint32_t getNode();
  bool isDoneEDT() { return IsDoneEDT; }

private:
  uint32_t Node = 0;
  EDTCallBase *Call = nullptr;
  EDT *Parent = nullptr;
  EDT *ParentSync = nullptr;
  EDT *DoneSync = nullptr;
  bool IsDoneEDT = false;
};

inline raw_ostream &operator<<(raw_ostream &OS, EDT &E) {
  OS << "- EDT #" << E.getID() << ": " << E.getName() << "\n";
  OS << "Ty: " << toString(E.getTy()) << "\n";
  OS << "Number of slots: " << E.getDepSlot() << "\n";
  OS << E.getDataEnv();

  return OS;
}

class TaskEDT : public EDT {
public:
  TaskEDT(EDTFunction *Fn);
  TaskEDT(EDTFunction *Fn, SmallVector<StringRef, 4> Inputs, SmallVector<StringRef, 4> Outputs);
  ~TaskEDT() = default;

  /// Getters
  int32_t getThreadNum() { return ThreadNum; }

  /// Insert
  void insertInput(EDT *E) { Inputs.push_back(E); }
  void insertOutput(EDT *E) { Outputs.push_back(E); }

  /// Static interface
  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_TASK) == EDTTypeKind::TK_TASK;
  }

private:
  int32_t ThreadNum = -1;
  SmallVector<EDT *> Inputs;
  SmallVector<EDT *> Outputs;
  /// Symbolic inputs and outputs
  SmallVector<StringRef, 4> SymInputs;
  SmallVector<StringRef, 4> SymOutputs;
};

class MainEDT : public TaskEDT {
public:
  MainEDT(EDTFunction *Fn);
  ~MainEDT() = default;

  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_MAIN) == EDTTypeKind::TK_MAIN;
  }
};

class SyncEDT : public TaskEDT {
public:
  SyncEDT(EDTFunction *Fn);
  ~SyncEDT() = default;

  /// Getters
  bool mustSyncChilds() const { return SyncChilds; }
  bool mustSyncDescendants() const { return SyncDescendants; }

  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_SYNC) == EDTTypeKind::TK_SYNC;
  }

protected:
  bool SyncChilds = true;
  bool SyncDescendants = true;
};

class ParallelEDT : public SyncEDT {
public:
  ParallelEDT(EDTFunction *Fn);
  ~ParallelEDT() = default;

  uint32_t getNumThreads() const { return NumThreads; }

  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_PARALLEL) ==
           EDTTypeKind::TK_PARALLEL;
  }

private:
  uint32_t NumThreads = 1;
  SetVector<Value *> Variables;
};

} // end namespace arts
#endif // LLVM_ARTS_H
