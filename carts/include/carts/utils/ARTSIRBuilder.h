//===----------------------------------------------------------------------===//
//
// This file defines the ARTSIRBuilder class and helpers used as a convenient
// way to generate ARTS IR Metadata
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_API_CARTS_ARTSIRBUILDER_H
#define LLVM_API_CARTS_ARTSIRBUILDER_H


#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Module.h"

#include "carts/analysis/ARTS.h"

namespace arts {
using namespace std;
using namespace llvm;

class EDTIRBuilder {
public:
  EDTIRBuilder(EDTType Ty) : Ty(Ty) {}
  ~EDTIRBuilder() {}

  void insertDep(Value *CallV, Value *FunctionV);
  void insertParam(Value *CallV, Value *FunctionV);
  void insertUnusedArg(Value *V);
  void buildEDT(Module &M, CallBase *OldCB, Function *OldFn);
  EDTType getTy() { return Ty; }

private:
  /// Set of inputs
  // Module &M;
  EDTType Ty;
  SmallSetVector<Value *, 4> CallArgs;
  // SmallSetVector<Value *, 4> NewFnArgs;
  /// Unused Arguments
  SmallSetVector<Value *, 4> UnusedArgs;
  /// Map to rewire values
  DenseMap<Value *, Value *> RewiringMap;
  /// Maps the function argument to an EDTArgType
  DenseMap<Value *, EDTArgType> ArgTypeMap;
};

/// An interface to create LLVM-IR for ARTS directives.
///
/// Each ARTS directive has a corresponding public generator method.
class ARTSIRBuilder {
public:
  /// Create a new ARTSIRBuilder operating on the given module \p M.
  // ARTSIRBuilder(Module &M) : M(M), Builder(M.getContext()) { initialize(); }
  ~ARTSIRBuilder() {}

  // /// ---------------------------- Interface ---------------------------- ///
  // /// Initialize the internal state, this will put structures types and
  // /// potentially other helpers into the underlying module. Must be called
  // /// before any other method and only once! This internal state includes
  // /// Types used in the ARTSIRBuilder generated from ARTSKinds.def
  // void initialize();

  // /// Finalize the underlying module, e.g., by outlining regions.
  // /// \param Fn                    The function to be finalized. If not used,
  // ///                              all functions are finalized.
  // void finalize();

  // /// Type used throughout for insertion points.
  // using InsertPointTy = IRBuilder<>::InsertPoint;

  // /// Return the function declaration for the runtime function with \p FnID.
  // FunctionCallee getOrCreateRuntimeFunction(Module &M,
  //                                           types::RuntimeFunction FnID);
  // Function *getOrCreateRuntimeFunctionPtr(types::RuntimeFunction FnID);

  // /// Interface to add ARTS methods
  // void initializeEDT(EDT &E);
  // void insertEDTEntry(EDT &E);
  // CallInst *insertEDTCall(EDT &E);
  // void reserveEDTGuid(EDT &E);

  /// ---------------------------- Private ---------------------------- ///
  /// Create all simple and struct types exposed by the runtime and remember
  /// the llvm::PointerTypes of them for easy access later.
  void initializeTypes();

  /// ---------------------------- Attributes ---------------------------- ///
  /// The underlying LLVM-IR module
  Module &M;
  /// The LLVM-IR Builder used to create IR.
  // IRBuilder<> Builder;
};

} // namespace arts

#endif // LLVM_API_CARTS_ARTSIRBUILDER_H