//===----------------------------------------------------------------------===//
//
// This file defines the ARTSCodegen class and helpers used as a convenient
// way to create LLVM instructions for ARTS directives.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ARTS_CODEGEN_H
#define LLVM_ARTS_CODEGEN_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include <cstdint>

#include "carts/analysis/graph/EDTGraph.h"
#include "carts/utils/ARTS.h"
#include "carts/utils/ARTSTypes.h"

namespace arts {
using namespace types;
struct ARTSCache;

/// Aux struct to hold generated code for an EDT.
class EDTCodegen {
public:
  EDTCodegen(EDT *E);
  EDTCodegen(EDT *E, Function *Fn);
  Function *getFn();
  CallBase *getCB();
  Value *getGuidAddress();
  void setFn(Function *Fn);
  void setCB(CallBase *CB);
  void setGuidAddress(Value *V);
  ///
  bool hasParameter(int32_t Slot);
  bool hasDependency(int32_t Slot);
  void insertParameter(int32_t Slot, Value *V);
  void insertDependency(int32_t Slot, Value *V);

private:
  EDT *E;
  CallBase *CB = nullptr;
  Function *Fn = nullptr;
  Value *GuidAddress = nullptr;
  /// Maps an integer slot to a value.
  DenseMap<int32_t, Value *> Parameters;
  DenseMap<int32_t, Value *> Dependencies;
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
  Value *getOrCreateEDTGuid(EDT &E);
  void initializeEDT(EDT &E);
  /// Based on the EDT parameters and dependencies, it "unrolls" the data
  /// structure and rewires the values to their new instances.
  void insertEDTEntry(EDT &E);
  void insertEDTCall(EDT &E);
  void signalEDTGuid(EDT &From, EDT &To, Value *Signal);
  void signalEDTValue(EDT &From, EDT &To, Value *Signal);
  void signalEDTDataBlock(EDT &From, EDT &To, Value *Signal);
  // Function *insertEDTFn(EDT &E);

  /// Handle interface
  void handleEDT(EDT &E);
  // void generateEDT(EDT &E);
  // void generateParallelEDT(EDT &E);
  // void generateTaskEDT(EDT &E);
  // void generateMainEDT(EDT &E);
  // void generateSyncEDT(EDT &E);

  /// Guid interface
  bool isEDTGuid(Value *V);

  /// ---------------------------- Utils ---------------------------- ///
  void setIPInEDTEntry(EDTFunction &EDTFn);

  /// Make \p Source branch to \p Target.
  ///
  /// Handles two situations:
  /// * \p Source already has an unconditional branch.
  /// * \p Source is a degenerate block (no terminator because the BB is
  ///             the current head of the IR construction).
  void redirectTo(BasicBlock *Source, BasicBlock *Target);
  // void redirectTo(Function *Source, BasicBlock *Target);
  void redirectExitsTo(Function *Source, BasicBlock *Target);
  void redirectEntryAndExit(EDT &E, BasicBlock *OriginalEntry);
  /// Insertion Points
  void setInsertPoint(BasicBlock *BB);
  void setInsertPoint(Instruction *I);

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
  EDTCodegen *getOrCreateEDT(EDT &E);
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
};
} // namespace arts

#endif // LLVM_ARTS_CODEGEN_H