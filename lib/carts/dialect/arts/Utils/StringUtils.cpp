///==========================================================================///
/// File: StringUtils.cpp
///
/// Utility queries for string-backed memrefs.
///==========================================================================///

#include "carts/dialect/arts/Utils/StringUtils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/DenseMap.h"

using namespace mlir;

namespace {
static bool isStringGlobal(LLVM::GlobalOp globalOp) {
  if (auto attr = globalOp.getValueOrNull())
    if (auto strAttr = dyn_cast<StringAttr>(attr))
      return strAttr.getValue().ends_with("\00");
  return false;
}

static bool isStringFunction(StringRef funcName) {
  const DenseSet<StringRef> stringFunctions = {
      "strlen", "strcpy", "strcat", "strcmp", "printf", "sprintf",
      "puts",   "fputs",  "scanf",  "fscanf", "gets"};
  return stringFunctions.contains(funcName);
}
} // namespace

void mlir::carts::arts::StringUtils::collectStringMemRefs(
    ModuleOp module, DenseSet<Value> &stringMemRefs) {
  if (!module)
    return;

  DenseMap<Value, LLVM::GlobalOp> globalSources;
  module.walk([&](LLVM::AddressOfOp addressOf) {
    auto globalOp =
        module.lookupSymbol<LLVM::GlobalOp>(addressOf.getGlobalName());
    if (!globalOp || !isStringGlobal(globalOp))
      return;

    SmallVector<Value, 8> worklist{addressOf.getResult()};
    while (!worklist.empty()) {
      Value current = worklist.pop_back_val();
      for (Operation *user : current.getUsers()) {
        if (!isa<LLVM::GEPOp, LLVM::LoadOp>(user))
          continue;
        for (Value result : user->getResults()) {
          globalSources.try_emplace(result, globalOp);
          worklist.push_back(result);
        }
      }
    }
  });

  module.walk([&](Operation *op) {
    if (auto affineStore = dyn_cast<affine::AffineStoreOp>(op)) {
      if (globalSources.count(affineStore.getValue()))
        stringMemRefs.insert(affineStore.getMemRef());
      return;
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (globalSources.count(store.getValueToStore()))
        stringMemRefs.insert(store.getMemRef());
    }
  });

  module.walk([&](func::CallOp callOp) {
    if (!isStringFunction(callOp.getCallee()))
      return;
    for (Value operand : callOp.getOperands()) {
      if (isa<MemRefType>(operand.getType())) {
        stringMemRefs.insert(operand);
        continue;
      }
      if (!isa<LLVM::LLVMPointerType>(operand.getType()))
        continue;
      Operation *defOp = operand.getDefiningOp();
      if (!defOp || !isa<polygeist::Pointer2MemrefOp>(defOp))
        continue;
      Value memrefOperand = defOp->getOperand(0);
      if (isa<MemRefType>(memrefOperand.getType()))
        stringMemRefs.insert(memrefOperand);
    }
  });
}

bool mlir::carts::arts::StringUtils::isStringMemRef(ModuleOp module,
                                                    Value value) {
  DenseSet<Value> stringMemRefs;
  collectStringMemRefs(module, stringMemRefs);
  return stringMemRefs.contains(value);
}
