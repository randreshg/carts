//===----------------------------------------------------------------------===//
//
// This file defines the ARTSIRBuilder class and helpers used as a convenient
// way to generate ARTS IR Metadata
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_API_CARTS_ARTSIRBUILDER_H
#define LLVM_API_CARTS_ARTSIRBUILDER_H

#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"

#include "carts/analysis/ARTS.h"

namespace arts {
using namespace std;
using namespace llvm;
using namespace arts::types;

/// ------------------------------------------------------------------- ///
///                          EDT IR BUILDER                             ///
/// An interface to create LLVM-IR for EDT directives.
/// ------------------------------------------------------------------- ///
class EDTIRBuilder {
public:
  EDTIRBuilder() {}
  EDTIRBuilder(EDTType Ty) : Ty(Ty) {}
  ~EDTIRBuilder() = default;

  void insertDep(Value *CallV);
  void insertParam(Value *CallV);
  void insertUnusedArg(Value *V);
  void insertMapValue(Value *OldV, Value *NewV);
  void setMetadata(Function &Fn);
  void setArgType(Value *FnArg, EDTArgType Ty);
  EDTType getTy() { return Ty; }
  void setMetadata();
  /// Builds the EDT and returns call instruction
  CallBase *buildEDT(
      CallBase *OldCB, Function *OldFn,
      function<void(EDTIRBuilder *, Function *, Function *)> fillRewiringMapFn,
      Instruction *InsertBefore = nullptr);

  // private:
  EDTType Ty;
  SmallVector<Value *, 16> UnusedArgs;
  DenseMap<Value *, Value *> RewiringMap;
  /// Maps the call argument to an EDTArgType
  SmallVector<Value *, 16> CallArgs;
  DenseMap<Value *, EDTArgType> CallArgTypeMap;
};

} // namespace arts

#endif // LLVM_API_CARTS_ARTSIRBUILDER_H