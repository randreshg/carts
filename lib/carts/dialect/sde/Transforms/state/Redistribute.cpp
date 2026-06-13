///==========================================================================///
/// File: Redistribute.cpp
///
/// SDE redistribution lowering.
///
/// `sde-layout-assignment` records committed producer/consumer layout facts on
/// accessing `sde.su_iterate` ops. This pass materializes movement where
/// producer and consumer layout disagree into first-class movement ops
/// (`sde.su_halo`, `sde.su_reduce_scatter`, or legacy `sde.redist` for
/// remaining families), or rejects it in SDE.
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

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace {

/// True if `root` already carries a redistribution fact equal to `edge`
/// (idempotence across re-runs and across consumers with the same target).
static bool alreadyRepresented(const carts::sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers())
    if (auto redist = dyn_cast<carts::sde::SdeRedistOp>(user))
      if (carts::sde::redistMatchesEdge(redist, edge))
        return true;
  for (Operation *user : edge.root.getUsers()) {
    if (auto halo = dyn_cast<carts::sde::SdeSuHaloOp>(user)) {
      if (edge.family == carts::sde::SdeMovementFamily::halo_like &&
          halo.getArrayIdAttr() &&
          halo.getArrayIdAttr().getInt() == edge.arrayId &&
          halo.getOwnerDims() == buildI64ArrayAttr(halo.getContext(),
                                                   edge.sourceOwnerDims) &&
          halo.getBlockShape() == buildI64ArrayAttr(halo.getContext(),
                                                    edge.sourceBlockShape) &&
          halo.getHaloShape() == buildI64ArrayAttr(halo.getContext(),
                                                   edge.haloShape))
        return true;
    }
    if (auto reduce =
            dyn_cast<carts::sde::SdeSuReduceScatterOp>(user)) {
      if (edge.family ==
              carts::sde::SdeMovementFamily::reduce_scatter_like &&
          reduce.getArrayIdAttr() &&
          reduce.getArrayIdAttr().getInt() == edge.arrayId &&
          reduce.getOwnerDims() == buildI64ArrayAttr(reduce.getContext(),
                                                     edge.sourceOwnerDims) &&
          reduce.getBlockShape() == buildI64ArrayAttr(reduce.getContext(),
                                                      edge.sourceBlockShape))
        return true;
    }
  }
  return false;
}

static std::optional<carts::sde::SdeReductionKind>
getFirstReductionKind(carts::sde::SdeSuIterateOp consumer) {
  ArrayAttr kinds = consumer.getReductionKindsAttr();
  if (!kinds || kinds.empty())
    return std::nullopt;
  if (auto attr =
          dyn_cast<carts::sde::SdeReductionKindAttr>(*kinds.begin()))
    return attr.getValue();
  return std::nullopt;
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

    for (const carts::sde::RedistributionEdge &edge : committed.edges) {
      carts::sde::SdeSuIterateOp consumer = edge.consumer;
      if (alreadyRepresented(edge))
        continue;
      auto parentDistribute =
          dyn_cast_or_null<carts::sde::SdeSuDistributeOp>(consumer->getParentOp());
      if (!parentDistribute) {
        consumer.emitOpError()
            << "sde-redistribute: movement edge for array " << edge.arrayId
            << " is not inside sde.su_distribute; refusing to emit an "
               "unscoped movement op";
        failed = true;
        continue;
      }
      IntegerAttr costAttr;
      if (edge.commVolumeBytes > 0)
        costAttr =
            IntegerAttr::get(IntegerType::get(ctx, 64), edge.commVolumeBytes);
      OpBuilder builder(consumer);
      IntegerAttr arrayIdAttr =
          IntegerAttr::get(IntegerType::get(ctx, 64), edge.arrayId);
      ArrayAttr ownerDims = buildI64ArrayAttr(ctx, edge.sourceOwnerDims);
      ArrayAttr blockShape = buildI64ArrayAttr(ctx, edge.sourceBlockShape);
      if (edge.family == carts::sde::SdeMovementFamily::halo_like) {
        carts::sde::SdeSuHaloOp::create(
            builder, consumer.getLoc(), edge.root, arrayIdAttr, ownerDims,
            blockShape, buildI64ArrayAttr(ctx, edge.haloShape));
        continue;
      }
      if (edge.family ==
          carts::sde::SdeMovementFamily::reduce_scatter_like) {
        carts::sde::SdeReductionKind kind =
            getFirstReductionKind(consumer).value_or(
                carts::sde::SdeReductionKind::add);
        carts::sde::SdeSuReduceScatterOp::create(
            builder, consumer.getLoc(), edge.root, arrayIdAttr, ownerDims,
            blockShape, IntegerAttr::get(IntegerType::get(ctx, 64), 0),
            carts::sde::SdeReductionKindAttr::get(ctx, kind));
        continue;
      }
      carts::sde::SdeRedistOp::create(
          builder, consumer.getLoc(), edge.root,
          carts::sde::SdeMovementFamilyAttr::get(ctx, edge.family), arrayIdAttr,
          ownerDims, blockShape, buildI64ArrayAttr(ctx, edge.targetOwnerDims),
          buildI64ArrayAttr(ctx, edge.targetBlockShape),
          edge.haloShape.empty() ? ArrayAttr()
                                 : buildI64ArrayAttr(ctx, edge.haloShape),
          /*commVolumeBytes=*/costAttr);
    }

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
