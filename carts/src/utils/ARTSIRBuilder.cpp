#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

#include "carts/analysis/ARTS.h"
#include "carts/utils/ARTSIRBuilder.h"
#include "carts/utils/ARTSUtils.h"

// DEBUG
#define DEBUG_TYPE "arts-ir-builder"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

// using namespace llvm;
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
    function<void(EDTIRBuilder *, Function *, Function *)> fillRewiringMapFn) {
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
  NewFn->setName("edt");
  OldFn->setSubprogram(nullptr);
  /// Splice the body of the old function right into the new function
  NewFn->getBasicBlockList().splice(NewFn->begin(), OldFn->getBasicBlockList());
  /// If any early return, remove terminator and return void
  for (auto &BB : *NewFn) {
    if (auto *TI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      TI->eraseFromParent();
      ReturnInst::Create(M.getContext(), nullptr, &BB);
    }
  }
  // LLVM_DEBUG(dbgs() << TAG << "Created new function: " << *NewFn << "\n");
  /// Fill the rewiring map and rewire the arguments
  fillRewiringMapFn(this, OldFn, NewFn);
  for (auto &MapItr : RewiringMap) {
    Value *OldV = MapItr.first;
    Value *NewV = MapItr.second;
    assert(OldV->getType() == NewV->getType() && "Types do not match");
    NewV->takeName(OldV);
    OldV->replaceAllUsesWith(NewV);
  }
  // LLVM_DEBUG(dbgs() << TAG << "New function after arguments were rewired: "
  //                   << *NewFn << "\n");
  /// Create new callsite.
  auto *NewCI = CallInst::Create(NewFnTy, NewFn, CallArgs, "", OldCB);
  /// Remove the old callsite
  if (OldCB->getType()->isVoidTy())
    OldCB->replaceAllUsesWith(NewCI);
  else
    replaceUsesWithUndef(OldCB, false, true, 1);
  OldCB->eraseFromParent();
  /// Remove unused arguments
  for (auto *UnusedArg : UnusedArgs)
    removeValue(UnusedArg, true, true);
  // LLVM_DEBUG(dbgs() << TAG << "New callsite: " << *NewCI << "\n");
  // LLVM_DEBUG(dbgs() << TAG << "New function: " << *NewFn << "\n");
  removeValue(OldFn, true, true);
  assert(!NewFn->isDeclaration() && "New function is a declaration");
  /// Set metadata
  setMetadata(*NewFn);
  return NewCI;
}

void EDTIRBuilder::setMetadata(Function &Fn) {
  LLVMContext &Ctx = Fn.getContext();
  auto TyStr = toString(Ty);
  /// Metadata Nodes for Argument Values
  SmallVector<Metadata *, 16> ArgMDs;
  for (auto *CallArg : CallArgs) {
    EDTArgType CallTy = CallArgTypeMap[CallArg];
    auto MDStr = "edt.arg." + toString(CallTy);
    ArgMDs.push_back( MDString::get(Ctx, MDStr));
  }
  MDNode *ArgNode = MDNode::get(Ctx, ArgMDs);
  /// Metadata Node for EDT
  SmallVector<Metadata *, 16> EDTMDs;
  EDTMDs.push_back(MDString::get(Ctx, "edt." + TyStr));
  EDTMDs.push_back(ArgNode);
  /// Set specific metadata for the function
  /// TODO: If it is parallel, add the number of threads...
  Fn.setMetadata("carts.metadata", MDNode::get(Ctx, EDTMDs));
}

void EDTIRBuilder::setArgType(Value *FnArg, EDTArgType Ty) {
  // CallArgTypeMap[FnArg] = Ty;
}
