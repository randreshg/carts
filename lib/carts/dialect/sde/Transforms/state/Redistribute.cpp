///==========================================================================///
/// File: Redistribute.cpp
///
/// SDE redistribution lowering.
///
/// `sde-layout-assignment` records temporary layout-disagreement markers on
/// accessing `sde.su_iterate` ops. This pass consumes each marker into
/// `sde.redist` structure, or rejects it in SDE.
///==========================================================================///

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool
removeRepresentedDisagreementIds(MLIRContext *ctx,
                                 carts::sde::SdeSuIterateOp consumer,
                                 ArrayRef<int64_t> representedIds) {
  ArrayAttr disagree = consumer.getLayoutsDisagreeAttr();
  if (!disagree || representedIds.empty())
    return false;

  llvm::DenseSet<int64_t> represented;
  for (int64_t id : representedIds)
    represented.insert(id);

  SmallVector<Attribute, 4> remaining;
  bool changed = false;
  for (Attribute attr : disagree) {
    auto idAttr = dyn_cast<IntegerAttr>(attr);
    if (idAttr && represented.contains(idAttr.getInt())) {
      changed = true;
      continue;
    }
    remaining.push_back(attr);
  }
  if (!changed)
    return false;
  if (remaining.empty())
    consumer->removeAttr(consumer.getLayoutsDisagreeAttrName());
  else
    consumer.setLayoutsDisagreeAttr(ArrayAttr::get(ctx, remaining));
  return true;
}

/// True if `root` already carries a redistribution fact equal to `edge`
/// (idempotence across re-runs and across consumers with the same target).
static bool alreadyRepresented(const carts::sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers())
    if (auto redist = dyn_cast<carts::sde::SdeRedistOp>(user))
      if (carts::sde::redistMatchesEdge(redist, edge))
        return true;
  return false;
}

struct SdeRedistributePass
    : public carts::sde::impl::SdeRedistributeBase<SdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    carts::sde::RedistributionEdges committed =
        carts::sde::collectRedistributionEdges(module);

    bool failed = false;
    for (const carts::sde::RedistributionEdgeFailure &failure :
         committed.failures) {
      carts::sde::SdeSuIterateOp consumer = failure.consumer;
      consumer.emitOpError()
          << "sde.redist: " << failure.reason << " (array " << failure.arrayId
          << "); refusing to invent redistribution";
      failed = true;
    }

    llvm::DenseMap<Operation *, SmallVector<int64_t, 4>> representedByConsumer;
    for (const carts::sde::RedistributionEdge &edge : committed.edges) {
      carts::sde::SdeSuIterateOp consumer = edge.consumer;
      representedByConsumer[consumer.getOperation()].push_back(edge.arrayId);
      if (alreadyRepresented(edge))
        continue;
      IntegerAttr costAttr;
      if (edge.commVolumeBytes > 0)
        costAttr =
            IntegerAttr::get(IntegerType::get(ctx, 64), edge.commVolumeBytes);
      OpBuilder builder(consumer);
      carts::sde::SdeRedistOp::create(
          builder, consumer.getLoc(), edge.root,
          carts::sde::SdeMovementFamilyAttr::get(ctx, edge.family),
          IntegerAttr::get(IntegerType::get(ctx, 64), edge.arrayId),
          buildI64ArrayAttr(ctx, edge.sourceOwnerDims),
          buildI64ArrayAttr(ctx, edge.sourceBlockShape),
          buildI64ArrayAttr(ctx, edge.targetOwnerDims),
          buildI64ArrayAttr(ctx, edge.targetBlockShape),
          edge.haloShape.empty() ? ArrayAttr()
                                 : buildI64ArrayAttr(ctx, edge.haloShape),
          /*commVolumeBytes=*/costAttr);
    }

    for (auto &entry : representedByConsumer)
      removeRepresentedDisagreementIds(
          ctx, cast<carts::sde::SdeSuIterateOp>(entry.first), entry.second);

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRedistributePass() {
  return std::make_unique<SdeRedistributePass>();
}
} // namespace mlir::carts::sde
