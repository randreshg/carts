///==========================================================================///
/// File: EdtAllocaSinking.cpp
///
/// Clone external stack allocas into EDT regions so that task outlining
/// captures a private copy instead of a shared pointer.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/utils/EdtUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::arts;

#define GEN_PASS_DEF_EDTALLOCASINKING
#include "arts/passes/Passes.h.inc"

namespace {

unsigned sinkExternalAllocasInEdt(EdtOp edt) {
  Block &body = edt.getBody().front();
  DenseMap<Operation *, SmallVector<Operation *, 4>> usesByAlloca;

  body.walk([&](Operation *op) {
    for (Value operand : op->getOperands()) {
      auto allocaOp = operand.getDefiningOp<memref::AllocaOp>();
      if (!allocaOp)
        continue;
      if (edt.getBody().isAncestor(allocaOp->getParentRegion()))
        continue;
      usesByAlloca[allocaOp.getOperation()].push_back(op);
    }
  });

  if (usesByAlloca.empty())
    return 0;

  OpBuilder builder(edt);
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&body);

  unsigned sunkAllocas = 0;
  for (const auto &entry : usesByAlloca) {
    auto allocaOp = cast<memref::AllocaOp>(entry.first);
    bool hasStoreInEdt = false;
    for (Operation *user : entry.second) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() == allocaOp.getResult()) {
          hasStoreInEdt = true;
          break;
        }
      }
    }

    bool hasUnsafeStore = false;
    SmallVector<memref::StoreOp, 4> initStores;
    for (Operation *user : allocaOp->getUsers()) {
      if (auto store = dyn_cast<memref::StoreOp>(user)) {
        if (store.getMemRef() != allocaOp.getResult())
          continue;
        /// Stores inside the EDT body are private writes (e.g., OpenMP
        /// private(buffer) lowered by cgeist outside omp.parallel). These
        /// should not prevent sinking — they are the reason we want to sink.
        if (edt.getBody().isAncestor(store->getParentRegion()))
          continue;
        if (!canCloneAllocaInitStore(store, allocaOp.getResult())) {
          hasUnsafeStore = true;
          break;
        }
        initStores.push_back(store);
        continue;
      }
    }

    if (hasUnsafeStore)
      continue;

    /// If the alloca is only read inside the EDT, sink it only when we can
    /// safely clone its initialization stores. This preserves scalar values
    /// that would otherwise be uninitialized after outlining.
    if (!hasStoreInEdt && initStores.empty())
      continue;

    Operation *clonedOp = builder.clone(*allocaOp.getOperation());
    ++sunkAllocas;
    auto newAlloca = cast<memref::AllocaOp>(clonedOp);
    IRMapping mapping;
    mapping.map(allocaOp.getResult(), newAlloca.getResult());

    for (Operation *user : entry.second)
      user->replaceUsesOfWith(allocaOp.getResult(), newAlloca.getResult());

    if (!initStores.empty()) {
      builder.setInsertionPointAfter(newAlloca);
      for (memref::StoreOp store : initStores)
        builder.clone(*store.getOperation(), mapping);
      builder.setInsertionPointToStart(&body);
    }
  }
  return sunkAllocas;
}

struct EdtAllocaSinkingPass
    : public ::impl::EdtAllocaSinkingBase<EdtAllocaSinkingPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    module.walk([&](EdtOp edt) { (void)sinkExternalAllocasInEdt(edt); });
  }
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass> createEdtAllocaSinkingPass() {
  return std::make_unique<EdtAllocaSinkingPass>();
}
} // namespace arts
} // namespace mlir
