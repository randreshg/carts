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
///                            EDT Metadata                             ///
/// ------------------------------------------------------------------- ///
// class EDTMetadata {
// public:
//   EDTMetadata(MDNode *Node) : Node(Node) {}
//   ~EDTMetadata() {}

// private:
//   /// MDNode for the EDT Metadata
//   MDNode *Node;
// };

// class ParallelEDTMetadata : public EDTMetadata {
// public:
//   ParallelEDTMetadata(MDNode *Node) : EDTMetadata(Node) {}
//   ~ParallelEDTMetadata() {}

//   uint32_t getNumberOfThreads() const;
//   void setNumberOfThreads(uint32_t NumThreads);
// };

// class TaskEDTMetadata : public EDTMetadata {
// public:
//   TaskEDTMetadata(MDNode *Node) : EDTMetadata(Node) {}
//   ~TaskEDTMetadata() {}

//   // uint32_t getNumberOfThreads() const;
// };

// class MainEDTMetadata : public EDTMetadata {
// public:
//   MainEDTMetadata(MDNode *Node) : EDTMetadata(Node) {}
//   ~MainEDTMetadata() {}

//   // uint32_t getNumberOfThreads() const;
// };

/// ------------------------------------------------------------------- ///
///                          ARTS IR BUILDER                            ///
/// An interface to create LLVM-IR for ARTS directives.
/// ------------------------------------------------------------------- ///
// class ARTSIRBuilder {
// public:
//   /// Create a new ARTSIRBuilder operating on the given module \p M.
//   // ARTSIRBuilder(Module &M) : M(M) {}
//   ARTSIRBuilder() {}
//   virtual ~ARTSIRBuilder() = default;
//   /// Set EDTMetadata
//   virtual void setMetadata();
// };

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
      Instruction *InsertBefore=nullptr);

  // private:
  EDTType Ty;
  SmallVector<Value *, 16> UnusedArgs;
  SmallVector<Value *, 16> CallArgs;
  DenseMap<Value *, Value *> RewiringMap;
  /// Maps the call argument to an EDTArgType
  DenseMap<Value *, EDTArgType> CallArgTypeMap;
};

} // namespace arts

#endif // LLVM_API_CARTS_ARTSIRBUILDER_H