#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

// #include "carts/utils/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSUtils.h"
#include "carts/utils/ARTSMetadata.h"

// DEBUG
#define DEBUG_TYPE "arts-ir-builder"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

using namespace llvm;
using namespace arts;
using namespace arts::utils;

/// ------------------------------------------------------------------- ///
///                          EDT IR BUILDER                             ///
/// ------------------------------------------------------------------- ///
void EDTIRBuilder::insertDep(Value *CallV) {
  CallArgs.push_back(CallV);
  CallArgTypeMap[CallV] = EDTArgType::Dep;
}

void EDTIRBuilder::insertParam(Value *CallV) {
  CallArgs.push_back(CallV);
  CallArgTypeMap[CallV] = EDTArgType::Param;
}

void EDTIRBuilder::insertMapValue(Value *OldV, Value *NewV) {
  RewiringMap[OldV] = NewV;
}
void EDTIRBuilder::insertUnusedArg(Value *V) { UnusedArgs.push_back(V); }

CallBase *EDTIRBuilder::buildEDT(
    CallBase *OldCB, Function *OldFn,
    function<void(EDTIRBuilder *, Function *, Function *)> fillRewiringMapFn,
    Instruction *InsertBefore) {
  LLVM_DEBUG(dbgs() << TAG << "Building EDT:\n");
  // LLVM_DEBUG(dbgs() << TAG << "Old function: " << *OldFn << "\n");
  /// Collect replacement argument types and copy over existing attributes.
  SmallVector<Type *, 16> NewArgumentTypes;
  for (auto Arg : CallArgs)
    NewArgumentTypes.push_back(Arg->getType());

  /// Construct the new function type using the new arguments types.
  Module &M = *OldFn->getParent();
  Type *RetTy = Type::getVoidTy(M.getContext());
  FunctionType *NewFnTy = FunctionType::get(RetTy, NewArgumentTypes, false);
  /// Create the new function body and insert it into the module.
  Function *NewFn = Function::Create(NewFnTy, OldFn->getLinkage(),
                                     OldFn->getAddressSpace(), "");
  OldFn->getParent()->getFunctionList().insert(OldFn->getIterator(), NewFn);
  NewFn->setName("carts.edt");
  OldFn->setSubprogram(nullptr);
  LLVM_DEBUG(dbgs() << "Created new function: " << *NewFn);
  /// Splice the body of the old function right into the new function
  NewFn->getBasicBlockList().splice(NewFn->begin(), OldFn->getBasicBlockList());
  /// If any early return, remove terminator and return void
  for (auto &BB : *NewFn) {
    if (auto *TI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      TI->eraseFromParent();
      ReturnInst::Create(M.getContext(), nullptr, &BB);
    }
  }
  /// Fill the rewiring map and rewire the arguments
  fillRewiringMapFn(this, OldFn, NewFn);
  LLVM_DEBUG(dbgs() << "Rewiring new function arguments:\n");
  for (auto &MapItr : RewiringMap) {
    Value *OldV = MapItr.first;
    Value *NewV = MapItr.second;
    assert(OldV->getType() == NewV->getType() && "Types do not match");
    LLVM_DEBUG(dbgs() << "  - Rewiring: " << *OldV << " -> " << *NewV << "\n");
    OldV->replaceAllUsesWith(NewV);
  }
  /// Create new callsite.
  CallInst *NewCI;
  if (InsertBefore)
    NewCI = CallInst::Create(NewFn, CallArgs, "", InsertBefore);
  else
    NewCI = CallInst::Create(NewFn, CallArgs, "", OldCB);
  /// Clean EDTIR after building the EDT
  LLVM_DEBUG(dbgs() << "New callsite: " << *NewCI << "\n");
  removeValue(OldCB, true, true);
  removeFunction(OldFn);
  // removeValue(OldFn, true, true);
  LLVM_DEBUG(dbgs() << "Current Function after undefining OldCB:\n"
                    << *(NewCI->getFunction()) << "\n");
  /// Undefine unused arguments
  for (auto *UnusedArg : UnusedArgs)
    UnusedArg->replaceAllUsesWith(UndefValue::get(UnusedArg->getType()));
  LLVM_DEBUG(dbgs() << TAG << "New function:\n" << *NewFn << "\n");
  assert(!NewFn->isDeclaration() && "New function is a declaration");
  /// Set metadata
  setMetadata(*NewFn);
  return NewCI;
}

void EDTIRBuilder::setMetadata(Function &Fn) {
  EDTMetadata::setMetadata(Fn, *this);
}