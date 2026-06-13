/// Redistribute.cpp
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/utils/ArrayAttrUtils.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
}
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include <memory>
using namespace mlir;
using namespace mlir::carts;
namespace {
static bool alreadyRepresented(const carts::sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers()) {
    if (auto halo = dyn_cast<carts::sde::SdeSuHaloOp>(user)) {
      if (edge.kind == carts::sde::RedistributionEdgeKind::Halo &&
          halo.getArrayIdAttr() && halo.getArrayIdAttr().getInt() == edge.arrayId &&
          halo.getOwnerDims() == buildI64ArrayAttr(halo.getContext(), edge.sourceOwnerDims) &&
          halo.getBlockShape() == buildI64ArrayAttr(halo.getContext(), edge.sourceBlockShape) &&
          halo.getHaloShape() == buildI64ArrayAttr(halo.getContext(), edge.haloShape))
        return true;
    }
    if (auto reduce = dyn_cast<carts::sde::SdeSuReduceScatterOp>(user)) {
      if (edge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter &&
          reduce.getArrayIdAttr() && reduce.getArrayIdAttr().getInt() == edge.arrayId &&
          reduce.getOwnerDims() == buildI64ArrayAttr(reduce.getContext(), edge.sourceOwnerDims) &&
          reduce.getBlockShape() == buildI64ArrayAttr(reduce.getContext(), edge.sourceBlockShape))
        return true;
    }
    if (auto allToAll = dyn_cast<carts::sde::SdeSuAllToAllOp>(user)) {
      if (edge.kind == carts::sde::RedistributionEdgeKind::AllToAll &&
          allToAll.getArrayIdAttr() &&
          allToAll.getArrayIdAttr().getInt() == edge.arrayId &&
          allToAll.getSourceOwnerDims() ==
              buildI64ArrayAttr(allToAll.getContext(), edge.sourceOwnerDims) &&
          allToAll.getSourceBlockShape() ==
              buildI64ArrayAttr(allToAll.getContext(), edge.sourceBlockShape) &&
          allToAll.getTargetOwnerDims() ==
              buildI64ArrayAttr(allToAll.getContext(), edge.targetOwnerDims) &&
          allToAll.getTargetBlockShape() ==
              buildI64ArrayAttr(allToAll.getContext(), edge.targetBlockShape))
        return true;
    }
  }
  return false;
}
static std::optional<carts::sde::SdeReductionKind>
getFirstReductionKind(carts::sde::SdeSuIterateOp consumer) {
  ArrayAttr kinds = consumer.getReductionKindsAttr();
  if (!kinds || kinds.empty()) return std::nullopt;
  if (auto attr = dyn_cast<carts::sde::SdeReductionKindAttr>(*kinds.begin()))
    return attr.getValue();
  return std::nullopt;
}
struct SdeRedistributePass : public carts::sde::impl::SdeRedistributeBase<SdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    carts::sde::RedistributionEdges committed = carts::sde::collectRedistributionEdges(module);
    bool failed = false;
    for (const auto &failure : committed.failures) {
      carts::sde::SdeSuIterateOp consumer = failure.consumer;
      consumer.emitOpError() << "sde-redistribute: " << failure.reason
          << " (array " << failure.arrayId << "); refusing to invent redistribution";
      failed = true;
    }
    for (const auto &edge : committed.edges) {
      if (alreadyRepresented(edge)) continue;
      auto consumer = edge.consumer;
      if (!dyn_cast_or_null<carts::sde::SdeSuDistributeOp>(consumer->getParentOp())) {
        consumer.emitOpError() << "sde-redistribute: movement edge for array " << edge.arrayId
            << " is not inside sde.su_distribute; refusing to emit an unscoped movement op";
        failed = true; continue;
      }
      IntegerAttr arrayIdAttr = IntegerAttr::get(IntegerType::get(ctx, 64), edge.arrayId);
      ArrayAttr ownerDims = buildI64ArrayAttr(ctx, edge.sourceOwnerDims);
      ArrayAttr blockShape = buildI64ArrayAttr(ctx, edge.sourceBlockShape);
      OpBuilder builder(consumer);
      if (edge.kind == carts::sde::RedistributionEdgeKind::Halo) {
        carts::sde::SdeSuHaloOp::create(builder, consumer.getLoc(), edge.root, arrayIdAttr,
            ownerDims, blockShape, buildI64ArrayAttr(ctx, edge.haloShape));
        continue;
      }
      if (edge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter) {
        auto kind = getFirstReductionKind(consumer).value_or(carts::sde::SdeReductionKind::add);
        carts::sde::SdeSuReduceScatterOp::create(builder, consumer.getLoc(), edge.root, arrayIdAttr,
            ownerDims, blockShape, IntegerAttr::get(IntegerType::get(ctx, 64), 0),
            carts::sde::SdeReductionKindAttr::get(ctx, kind));
        continue;
      }
      if (edge.kind == carts::sde::RedistributionEdgeKind::AllToAll) {
        carts::sde::SdeSuAllToAllOp::create(
            builder, consumer.getLoc(), edge.root, arrayIdAttr,
            buildI64ArrayAttr(ctx, edge.sourceOwnerDims),
            buildI64ArrayAttr(ctx, edge.sourceBlockShape),
            buildI64ArrayAttr(ctx, edge.targetOwnerDims),
            buildI64ArrayAttr(ctx, edge.targetBlockShape));
        continue;
      }
      consumer.emitOpError() << "sde-redistribute: unexpected redistribution edge kind";
      failed = true;
    }
    if (failed) signalPassFailure();
  }
};
}
namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRedistributePass() {
  return std::make_unique<SdeRedistributePass>();
}
}
