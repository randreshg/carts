//===- ARTS.h - ARTS-Related structs ----------------------------*- C++ -*-===//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_H
#define LLVM_ARTS_H

#include "llvm/ADT/SetVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <sys/types.h>

#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace llvm;
using namespace std;

class EDT;
class DataBlock;

using EDTValue = Value;
using EDTValueSet = SetVector<EDTValue *>;
using EDTCallBase = CallBase;
using EDTFunction = Function;
using EDTFunctionSet = SetVector<EDTFunction *>;
using EDTSet = SetVector<EDT *>;
using DataBlockSet = SetVector<DataBlock *>;

/// ------------------------------------------------------------------- ///
///                              DATA BLOCK                            ///
/// It represents the dependencies that are required for the EDT to run.
/// It contains the following information:
/// - V is the value that is being passed to the EDT
/// - M is the mode of the DataBlock (ReadOnly, WriteOnly, ReadWrite)
/// - ContextEDT is the EDT that is using DataBlock
/// - Parent is the parent DataBlock
/// - ChildrenDB are the children data blocks that are using the data block
/// - Done is the data block that is used to signal the completion of the
///   data block
/// - Slot is the slot number of the data block
/// - ToSlot is the slot number of the data block that is used to signal the
///   completion of the data block
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
class EDT {
public:
  EDT(EDTFunction *Fn);
  virtual ~EDT();

  /// Static interface
  static uint32_t Counter;
  static EDT *get(EDTFunction *Fn);
  static void setMetadata(EDTIRBuilder &Builder);
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
  /// Maps a DepSlot to a Function Argument
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
  ~TaskEDT() = default;

  /// Getters
  int32_t getThreadNum() { return ThreadNum; }

  /// Static interface
  static void setMetadata(EDTIRBuilder &Builder, int32_t ThreadNum = -1);
  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_TASK) == EDTTypeKind::TK_TASK;
  }

private:
  int32_t ThreadNum = -1;
};

class MainEDT : public TaskEDT {
public:
  MainEDT(EDTFunction *Fn);
  ~MainEDT() = default;

  static void setMetadata(EDTIRBuilder &Builder);
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

  static void setMetadata(EDTIRBuilder &Builder, SetVector<EDT *> &Inputs,
                          SetVector<EDT *> &Outputs, bool SyncChilds = true,
                          bool SyncDescendants = false, int32_t ThreadNum = -1);
  static bool classof(const EDT *E) {
    return (E->getTypeKind() & EDTTypeKind::TK_SYNC) == EDTTypeKind::TK_SYNC;
  }

protected:
  bool SyncChilds = true;
  bool SyncDescendants = true;
  SetVector<EDT *> Inputs;
  SetVector<EDT *> Outputs;
};

class ParallelEDT : public SyncEDT {
public:
  ParallelEDT(EDTFunction *Fn);
  ~ParallelEDT() = default;

  uint32_t getNumThreads() const { return NumThreads; }

  static void setMetadata(EDTIRBuilder &Builder, int32_t ThreadNum = -1,
                          uint32_t NumThreads = 1);
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
