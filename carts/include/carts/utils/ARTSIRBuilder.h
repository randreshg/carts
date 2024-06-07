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

  void insertDep(Value *CallV);
  void insertParam(Value *CallV);
  void insertUnusedArg(Value *V);
  void insertMapValue(Value *OldV, Value *NewV);
  /// Builds the EDT and returns call instruction
  CallBase *buildEDT(
      CallBase *OldCB, Function *OldFn,
      function<void(EDTIRBuilder *, Function *, Function *)> fillRewiringMapFn);
  EDTType getTy() { return Ty; }

// private:
  /// Set of inputs
  // Module &M;
  EDTType Ty;
  /// Unused Arguments
  SmallVector<Value *, 16> UnusedArgs;
  /// Call Arguments
  SmallVector<Value *, 16> CallArgs;
  // SmallSetVector<Value *, 4> NewFnArgs;

  /// Map to rewire values
  DenseMap<Value *, Value *> RewiringMap;
  /// Maps the call argument to an EDTArgType
  DenseMap<Value *, EDTArgType> CallArgTypeMap;
};

/// An interface to create LLVM-IR for ARTS directives.
class ARTSIRBuilder {
public:
  /// Create a new ARTSIRBuilder operating on the given module \p M.
  ARTSIRBuilder(Module &M) : M(M) {}
  ~ARTSIRBuilder() {}

  /// ---------------------------- Attributes ---------------------------- ///
  /// The underlying LLVM-IR module
  Module &M;
  /// The LLVM-IR Builder used to create IR.
  // IRBuilder<> Builder;
};

} // namespace arts

#endif // LLVM_API_CARTS_ARTSIRBUILDER_H