//===----------------------------------------------------------------------===//
//
// This file defines the ARTSIRBuilder class and helpers used as a convenient
// way to generate ARTS IR Metadata
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_API_CARTS_ARTSIRBUILDER_H
#define LLVM_API_CARTS_ARTSIRBUILDER_H

#include "carts/utils/ARTSTypes.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

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
  EDTIRBuilder(EDTType Ty) : Ty(Ty) {}
  EDTIRBuilder(EDTType Ty, Function *Fn) : Ty(Ty), NewFn(Fn) {}
  ~EDTIRBuilder() = default;

  void insertDep(Value *CallV);
  void insertParam(Value *CallV);
  void insertUnusedArg(Value *V);
  void insertRewireValue(Value *OldV, Value *NewV);

  /// Getters
  Function *getNewFn();
  EDTType getEDTType();
  EDTArgType getArgType(Value *V);
  SmallVector<Value *, 16> &getCallArgs();

  /// Setters
  void setNewFn(Function *Fn);

  /// Builds the EDT and returns call instruction
  CallBase *buildEDT(
      CallBase *OldCB, Function *OldFn,
      function<void(EDTIRBuilder *, Function *, Function *)> fillRewiringMapFn,
      Instruction *InsertBefore = nullptr, std::string EDTName = CARTS_EDT);

private:
  EDTType Ty;
  Function *NewFn = nullptr;
  SmallVector<Value *, 16> UnusedArgs;
  /// Maps the old value to the new value
  DenseMap<Value *, Value *> RewiringMap;
  /// Maps the call argument to an EDTArgType
  SmallVector<Value *, 16> CallArgs;
  DenseMap<Value *, EDTArgType> CallArgTypeMap;
};

} // namespace arts

#endif // LLVM_API_CARTS_ARTSIRBUILDER_H