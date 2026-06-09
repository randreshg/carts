///==========================================================================///
/// File: ReductionAtomicMaterialization.cpp
///
/// Materialize explicit atomic reductions inside CODIR codelets.
///==========================================================================///

#include "carts/dialect/codir/Transforms/Passes.h"

#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/Statistic.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_REDUCTIONATOMICMATERIALIZATION
#include "carts/dialect/codir/Transforms/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

static llvm::Statistic numAtomicReductionsMaterialized{
    "codir_reduction_atomic_materialization",
    "NumAtomicReductionsMaterialized",
    "Number of explicit CODIR atomic reductions materialized"};

namespace {

static bool sameMemrefAccess(Value lhsMemref, ValueRange lhsIndices,
                             Value rhsMemref, ValueRange rhsIndices) {
  return lhsMemref == rhsMemref &&
         ValueAnalysis::areValueRangesIdentical(lhsIndices, rhsIndices);
}

static bool isKnownZeroIndex(Value value) {
  if (std::optional<int64_t> constant =
          ValueAnalysis::tryFoldConstantIndex(value))
    return *constant == 0;
  return false;
}

static bool isAtomicAddAddressable(Value memref, ValueRange indices) {
  auto memrefType = dyn_cast<MemRefType>(memref.getType());
  if (!memrefType)
    return false;

  Value root = ValueAnalysis::stripMemrefViewOps(memref);
  Operation *rootDef = root ? root.getDefiningOp() : nullptr;
  if (isa_and_nonnull<memref::AllocOp, memref::AllocaOp>(rootDef))
    return false;

  if (memrefType.getRank() == 0)
    return indices.empty();
  if (memrefType.getRank() != 1 || indices.size() != 1)
    return false;
  return isKnownZeroIndex(indices.front());
}

static bool shouldMaterializeAtomic(codir::CodeletOp codelet) {
  if (codelet.getPartialReductionAttr())
    return false;
  auto strategy = codelet.getReductionStrategyAttr();
  return strategy &&
         strategy.getValue() == codir::CodirReductionStrategy::atomic;
}

static unsigned materializeAtomicUpdates(codir::CodeletOp codelet) {
  SmallVector<memref::StoreOp, 8> stores;
  codelet.getBody().walk([&](memref::StoreOp store) {
    stores.push_back(store);
  });

  unsigned lowered = 0;
  for (memref::StoreOp store : stores) {
    auto add = store.getValue().getDefiningOp<arith::AddIOp>();
    if (!add || !add->hasOneUse())
      continue;

    memref::LoadOp load;
    Value increment;
    for (Value operand : add->getOperands()) {
      auto candidate = operand.getDefiningOp<memref::LoadOp>();
      if (!candidate)
        continue;
      if (!sameMemrefAccess(candidate.getMemref(), candidate.getIndices(),
                            store.getMemref(), store.getIndices()))
        continue;
      load = candidate;
      increment = add.getLhs() == operand ? add.getRhs() : add.getLhs();
      break;
    }
    if (!load || !load->hasOneUse() || !increment)
      continue;
    if (!isAtomicAddAddressable(store.getMemref(), store.getIndices()))
      continue;

    OpBuilder builder(store);
    codir::AtomicAddOp::create(builder, store.getLoc(), store.getMemref(),
                               increment);
    store.erase();
    if (add->use_empty())
      add.erase();
    if (load->use_empty())
      load.erase();
    ++lowered;
  }

  return lowered;
}

struct ReductionAtomicMaterializationPass
    : public codir::impl::ReductionAtomicMaterializationBase<
          ReductionAtomicMaterializationPass> {
  void runOnOperation() override {
    getOperation().walk([&](codir::CodeletOp codelet) {
      if (!shouldMaterializeAtomic(codelet))
        return;
      numAtomicReductionsMaterialized += materializeAtomicUpdates(codelet);
    });
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::codir::createReductionAtomicMaterializationPass() {
  return std::make_unique<ReductionAtomicMaterializationPass>();
}
