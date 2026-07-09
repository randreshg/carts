///==========================================================================///
/// File: SdeMovementSynthesis.cpp
///
/// Phase C vertical slice: synthesize first-class SDE movement structure from
/// grounded committed layout facts. This initial slice realizes only stencil
/// halo movement and fails closed for unsupported movement families.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEMOVEMENTSYNTHESIS
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/Analysis/SdeAccessRelation.h"
#include "carts/dialect/sde/Analysis/SdeCommVolumeCost.h"
#include "carts/dialect/sde/Analysis/SdeDependence.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

static bool sameArrayAttr(ArrayAttr attr, ArrayRef<int64_t> values) {
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(attr);
  return parsed && ArrayRef<int64_t>(*parsed) == values;
}

static bool alreadyRepresented(const sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers()) {
    auto halo = dyn_cast<sde::SdeSuHaloOp>(user);
    if (!halo || !halo.getArrayIdAttr() ||
        halo.getArrayIdAttr().getInt() != edge.arrayId)
      continue;
    if (sameArrayAttr(halo.getOwnerDims(), edge.sourceOwnerDims) &&
        sameArrayAttr(halo.getBlockShape(), edge.sourceBlockShape) &&
        sameArrayAttr(halo.getHaloShape(), edge.haloShape))
      return true;
  }
  return false;
}

static bool ensureMovementScope(sde::SdeSuIterateOp consumer,
                                sde::RedistributionEdgeKind kind) {
  if (consumer->getParentOfType<sde::SdeSuDistributeOp>())
    return true;
  if (consumer.getNumResults() != 0)
    return false;

  sde::SdeDistributionKind distributionKind =
      kind == sde::RedistributionEdgeKind::Halo
          ? sde::SdeDistributionKind::owner_compute
          : sde::SdeDistributionKind::blocked;
  IRRewriter rewriter(consumer.getContext());
  rewriter.setInsertionPoint(consumer);
  auto distribute = sde::SdeSuDistributeOp::create(
      rewriter, consumer.getLoc(),
      sde::SdeDistributionKindAttr::get(consumer.getContext(),
                                        distributionKind));
  Block &body = sde::ensureBlock(distribute.getBody());
  consumer->moveBefore(&body, body.end());
  return true;
}

struct SdeMovementSynthesisPass
    : public sde::impl::SdeMovementSynthesisBase<SdeMovementSynthesisPass> {
  void runOnOperation() override {
    sde::SdeAccessRelation &accessRelations =
        getAnalysis<sde::SdeAccessRelation>();
    sde::SdeCommVolumeCost &commCost = getAnalysis<sde::SdeCommVolumeCost>();
    sde::SdeDependence &dependence = getAnalysis<sde::SdeDependence>();
    (void)accessRelations;
    (void)commCost;
    (void)dependence;

    ModuleOp module = getOperation();
    sde::RedistributionEdges committed =
        sde::collectRedistributionEdges(module);

    bool sawFailure = false;
    for (sde::RedistributionEdgeFailure &failure : committed.failures) {
      sde::SdeSuIterateOp consumer = failure.consumer;
      consumer.emitOpError()
          << "sde-movement-synthesis: " << failure.reason << " (array "
          << failure.arrayId << "); refusing to invent movement";
      sawFailure = true;
    }

    MLIRContext *ctx = &getContext();
    for (sde::RedistributionEdge &edge : committed.edges) {
      sde::SdeSuIterateOp consumer = edge.consumer;
      if (edge.kind != sde::RedistributionEdgeKind::Halo) {
        consumer.emitOpError()
            << "sde-movement-synthesis: unsupported movement family for array "
            << edge.arrayId
            << "; reduce-scatter/all-to-all require a backed synthesis slice";
        sawFailure = true;
        continue;
      }
      if (alreadyRepresented(edge))
        continue;
      if (!ensureMovementScope(consumer, edge.kind) ||
          !consumer->getParentOfType<sde::SdeSuDistributeOp>()) {
        consumer.emitOpError()
            << "sde-movement-synthesis: halo movement for array "
            << edge.arrayId
            << " is not inside sde.su_distribute; refusing unscoped movement";
        sawFailure = true;
        continue;
      }

      OpBuilder builder(consumer);
      sde::SdeSuHaloOp::create(
          builder, consumer.getLoc(), edge.root,
          IntegerAttr::get(IntegerType::get(ctx, 64), edge.arrayId),
          buildI64ArrayAttr(ctx, edge.sourceOwnerDims),
          buildI64ArrayAttr(ctx, edge.sourceBlockShape),
          buildI64ArrayAttr(ctx, edge.haloShape));
    }

    if (sawFailure)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::sde::createSdeMovementSynthesisPass() {
  return std::make_unique<SdeMovementSynthesisPass>();
}
