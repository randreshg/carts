
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

#include "carts/utils/ARTSIRBuilder.h"


// #include <cassert>

// DEBUG
#define DEBUG_TYPE "arts-ir-builder"
#if !defined(NDEBUG)
static constexpr auto TAG = "[" DEBUG_TYPE "] ";
#endif

// using namespace llvm;
using namespace arts;
// using namespace arts::types;

void EDTIRBuilder::insertDep(Value *CallV, Value *FunctionV) {
    CallArgs.insert(CallV);
    ArgTypeMap[FunctionV] = EDTArgType::Dep;
    RewiringMap[FunctionV] = CallV;
  }

  void EDTIRBuilder::insertParam(Value *CallV, Value *FunctionV) {
    CallArgs.insert(CallV);
    ArgTypeMap[FunctionV] = EDTArgType::Param;
    RewiringMap[FunctionV] = CallV;
  }

  void EDTIRBuilder::insertUnusedArg(Value *V) { UnusedArgs.insert(V); }

  void EDTIRBuilder::buildEDT(Module &M, CallBase *OldCB, Function *OldFn) {
    /// Generate new outlined function
    SmallVector<Type *, 16> NewArgumentTypes;

    // Collect replacement argument types and copy over existing attributes.
    for (auto Arg : CallArgs)
      NewArgumentTypes.push_back(Arg->getType());

    // Construct the new function type using the new arguments types.
    /// Get void return
    Type *RetTy = Type::getVoidTy(M.getContext());
    FunctionType *NewFnTy = FunctionType::get(RetTy, NewArgumentTypes, false);

    // Create the new function body and insert it into the module.
    Function *NewFn = Function::Create(NewFnTy, OldFn->getLinkage(),
                                       OldFn->getAddressSpace(), "");
    OldFn->getParent()->getFunctionList().insert(OldFn->getIterator(), NewFn);
    // NewFn->takeName(OldFn);
    NewFn->setName("parallel.edt");
    OldFn->setSubprogram(nullptr);

    /// Eliminate the instructions *after* we visited all of them.
    // OldCB->replaceAllUsesWith(NewCB);
    // OldCB->eraseFromParent();
    // ValuesToRemove.push_back(OldFn);

    /// Since we have now created the new function, splice the body of the old
    /// function right into the new function, leaving the old rotting hulk
    /// of the function empty.
  }