//===----------------------------------------------------------------------===//
//
// This file defines the ARTSCodegen class and helpers used as a convenient
// way to create LLVM instructions for ARTS directives.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include <cstdint>

#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace types;
struct ARTSCache;

class EDTSlotCodegen {
public:
  EDTSlotCodegen(Value *GuidAddress, Value *Ptr);
  Value *getGuidAddress();
  void setGuidAddress(Value *V);
  Value *getPtr();
  void setPtr(Value *V);

private:
  Value *GuidAddress = nullptr;
  Value *Ptr = nullptr;
};

/// Aux struct to hold generated code for an EDT.
class EDTCodegen {
public:
  EDTCodegen(EDT *E);
  EDTCodegen(EDT *E, Function *Fn);
  ~EDTCodegen();
  Function *getFn();
  CallBase *getCB();
  Value *getGuidAddress();

  void setFn(Function *Fn);
  void setCB(CallBase *CB);
  void setGuidAddress(Value *V);

  /// Entry and Exit
  BasicBlock *getEntry();
  BasicBlock *getExit();
  void setEntry(BasicBlock *BB);
  void setExit(BasicBlock *BB);

  /// Parameters
  Value *getParameter(uint32_t Slot);
  bool insertParameter(uint32_t Slot, Value *V = nullptr);
  /// Dependencies
  EDTSlotCodegen *getDependency(uint32_t Slot);
  Value *getDependencyGuid(uint32_t Slot);
  Value *getDependencyPtr(uint32_t Slot);
  bool insertDependency(uint32_t Slot, Value *GuidAddress, Value *Ptr);
  /// Guids
  Value *getGuid(EDT *E);
  bool insertGuid(EDT *E, Value *V = nullptr);

private:
  EDT *E;
  CallBase *CB = nullptr;
  Function *Fn = nullptr;
  Value *GuidAddress = nullptr;
  /// Entry and Exit
  BasicBlock *Entry = nullptr;
  BasicBlock *Exit = nullptr;
  /// Maps of known parameter values
  DenseMap<int32_t, Value *> Parameters;
  /// Maps of known dependency values
  DenseMap<int32_t, EDTSlotCodegen *> Dependencies;
  /// Maps of known guid values
  DenseMap<EDT *, Value *> Guids;
};

class DBCodegen {
public:
  DBCodegen(DataBlock *DB);
  Value *getGuidAddress();
  void setGuidAddress(Value *V);
  Value *getPtr();
  void setPtr(Value *V);

private:
  DataBlock *DB;
  Value *GuidAddress = nullptr;
  Value *Ptr = nullptr;
};

/// An interface to create LLVM-IR for ARTS directives.
/// Each ARTS directive has a corresponding public generator method.
class ARTSCodegen {
public:
  /// Create a new ARTSCodegen operating on the given module \p M.
  ARTSCodegen(ARTSCache *Cache);
  ~ARTSCodegen();

  /// ---------------------------- Interface ---------------------------- ///
  /// Initialize the internal state, this will put structures types and
  /// potentially other helpers into the underlying module. Must be called
  /// before any other method and only once! This internal state includes
  /// Types used in the ARTSCodegen generated from ARTSKinds.def
  void initialize();

  /// Finalize the underlying module, e.g., by outlining regions.
  /// \param Fn  The function to be finalized. If not used,
  ///            all functions are finalized.
  void finalize();

  /// Type used throughout for insertion points.
  using InsertPointTy = IRBuilder<>::InsertPoint;

  /// Return the function declaration for the runtime function with \p FnID.
  FunctionCallee getOrCreateRuntimeFunction(types::RuntimeFunction FnID);
  Function *getOrCreateRuntimeFunctionPtr(types::RuntimeFunction FnID);

  /// Interface to add ARTS methods
  Function *getOrCreateEDTFunction(EDT &E);
  Value *createEDTGuid(string EDTName, uint32_t EDTNode);
  Value *getOrCreateEDTGuid(EDT &E, BasicBlock *InsertionBB = nullptr);

  /// Based on the EDT parameters and dependencies, it "unrolls" the data
  /// structure and rewires the values to their new instances.
  void insertEDTEntry(EDT &E);
  void insertEDTCall(EDT &E);
  void insertEDTSignals(EDT &E);
  void createInitPerNodeFn();
  void createInitPerWorkerFn();
  void createMainFn();
  void insertInitFunctions();
  void insertARTSShutdownFn();

  /// ---------------------------- Utils ---------------------------- ///
  void insertPrint(string FormatStr, SmallVector<Value *, 8> &Args);
  void setInsertionPointInBB(BasicBlock &InsertionBB);

  /// Make \p Source branch to \p Target.
  ///
  /// Handles two situations:
  /// * \p Source already has an unconditional branch.
  /// * \p Source is a degenerate block (no terminator because the BB is
  ///             the current head of the IR construction).
  void redirectTo(BasicBlock *Source, BasicBlock *Target);
  void redirectExitsTo(Function *Source, BasicBlock *Target);
  /// Insertion Points
  void setInsertPoint(BasicBlock *BB);
  void setInsertPoint(Instruction *I);
  /// Rewired Values
  void rewireValues();
  void insertRewiredValue(Value *Old, Value *New);
  Value *getRewiredValue(Value *Old);
  Value *getValue(Value *V);


/// ---------------------------- Types ---------------------------- ///
/// Declarations for LLVM-IR types (simple, array, function and structure) are
/// generated below. Their names are defined and used in ARTSKinds.def. Here
/// we provide the declarations, the initializeTypes function will provide the
/// values.
///
///{
#define ARTS_TYPE(VarName, InitValue) Type *VarName = nullptr;
#define ARTS_ARRAY_TYPE(VarName, ElemTy, ArraySize)                            \
  ArrayType *VarName##Ty = nullptr;                                            \
  PointerType *VarName##PtrTy = nullptr;
#define ARTS_FUNCTION_TYPE(VarName, IsVarArg, ReturnType, ...)                 \
  FunctionType *VarName = nullptr;                                             \
  PointerType *VarName##Ptr = nullptr;
#define ARTS_STRUCT_TYPE(VarName, StrName, ...)                                \
  StructType *VarName = nullptr;                                               \
  PointerType *VarName##Ptr = nullptr;
#include "ARTSKinds.def"
  ///}
private:
  /// ---------------------------- Private ---------------------------- ///
  EDTCodegen *getOrCreateEDTCodegen(EDT &E);
  DBCodegen *getOrCreateDBCodegen(DataBlock &DB);
  /// Create all simple and struct types exposed by the runtime and remember
  /// the llvm::PointerTypes of them for easy access later.
  void initializeTypes();

  /// ---------------------------- Attributes ---------------------------- ///
  /// ARTS Cache Information
  ARTSCache *Cache;
  /// The module we are operating on.
  Module &M;
  /// The LLVM-IR Builder used to create IR.
  IRBuilder<> Builder;
  /// Maps the EDT to the new function
  DenseMap<EDT *, EDTCodegen *> EDTs;
  /// Maps the value to the new DataBlock
  DenseMap<DataBlock *, DBCodegen *> DBs;
  /// Rewired values
  DenseMap<Value *, Value *> RewiredValues;
};
} // namespace arts

#endif // LLVM_ARTS_CODEGEN_H