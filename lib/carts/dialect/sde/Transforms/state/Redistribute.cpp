/// Redistribute.cpp
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
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

struct ReduceScatterWriteTarget {
  Value root;
  int64_t arrayId = -1;
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

static bool endpointShapeFitsRoot(Value root, ArrayRef<int64_t> ownerDims,
                                  ArrayRef<int64_t> blockShape) {
  auto type = dyn_cast<MemRefType>(root.getType());
  if (!type || !type.hasStaticShape() || ownerDims.empty() ||
      blockShape.empty())
    return false;
  int64_t rank = type.getRank();
  if (static_cast<int64_t>(blockShape.size()) != rank &&
      blockShape.size() != ownerDims.size())
    return false;
  SmallVector<bool, 4> isOwner(rank, false);
  for (int64_t dim : ownerDims) {
    if (dim < 0 || dim >= rank || isOwner[dim])
      return false;
    isOwner[dim] = true;
  }
  ArrayRef<int64_t> shape = type.getShape();
  if (static_cast<int64_t>(blockShape.size()) == rank) {
    for (int64_t dim = 0; dim < rank; ++dim) {
      int64_t extent = blockShape[dim];
      if (extent <= 0 || extent > shape[dim])
        return false;
      if (!isOwner[dim] && extent != shape[dim])
        return false;
    }
    return true;
  }
  for (auto [slot, dim] : llvm::enumerate(ownerDims)) {
    int64_t extent = blockShape[slot];
    if (extent <= 0 || extent > shape[dim])
      return false;
  }
  return true;
}

static FailureOr<std::optional<ReduceScatterWriteTarget>>
findReduceScatterWriteTarget(carts::sde::SdeSuIterateOp consumer) {
  std::optional<ReduceScatterWriteTarget> target;
  if (ArrayAttr layout = consumer.getArrayLayoutAttr()) {
    for (const carts::sde::LayoutGraphFact &fact :
         carts::sde::parseArrayLayoutFacts(layout)) {
      if (fact.role != carts::sde::LayoutGraphRole::write ||
          fact.ownerDims.empty())
        continue;
      Value root = carts::sde::findArrayLayoutRoot(
          consumer, fact.id, carts::sde::SdeAccessMode::write);
      if (!root)
        continue;
      ReduceScatterWriteTarget candidate;
      candidate.root = root;
      candidate.arrayId = fact.id;
      candidate.ownerDims.assign(fact.ownerDims.begin(), fact.ownerDims.end());
      ArrayRef<int64_t> blockShape = fact.budgetBlockShape.empty()
                                         ? ArrayRef<int64_t>(fact.blockShape)
                                         : ArrayRef<int64_t>(fact.budgetBlockShape);
      candidate.blockShape.assign(blockShape.begin(), blockShape.end());
      if (!endpointShapeFitsRoot(candidate.root, candidate.ownerDims,
                                 candidate.blockShape)) {
        auto type = dyn_cast<MemRefType>(candidate.root.getType());
        if (type && type.hasStaticShape()) {
          SmallVector<int64_t, 4> projected(type.getShape().begin(),
                                            type.getShape().end());
          for (int64_t ownerDim : candidate.ownerDims)
            if (ownerDim >= 0 && ownerDim < type.getRank())
              projected[ownerDim] = 1;
          if (endpointShapeFitsRoot(candidate.root, candidate.ownerDims,
                                    projected))
            candidate.blockShape = std::move(projected);
        }
      }
      if (target)
        return failure();
      target = std::move(candidate);
    }
  }
  return target;
}

static bool isElementwisePipeline(carts::sde::SdeSuIterateOp consumer) {
  std::optional<carts::sde::SdeStructuredClassification> classification =
      consumer.getStructuredClassification();
  return classification &&
         *classification ==
             carts::sde::SdeStructuredClassification::elementwise_pipeline;
}

static LogicalResult ensurePartialReductionFacts(
    carts::sde::SdeSuIterateOp consumer,
    const ReduceScatterWriteTarget &target) {
  MLIRContext *ctx = consumer.getContext();
  SmallVector<int64_t, 4> reductionDims;
  std::optional<carts::sde::SuLoopAccessSummary> summary =
      carts::sde::analyzeSuLoopAccesses(consumer);
  if (!summary)
    return failure();
  for (auto [dim, iteratorType] : llvm::enumerate(summary->iterTypes))
    if (iteratorType == utils::IteratorType::reduction)
      reductionDims.push_back(static_cast<int64_t>(dim));
  if (reductionDims.empty() || target.ownerDims.empty())
    return failure();

  consumer.setPartialReductionAttr(UnitAttr::get(ctx));
  consumer.setPartialReductionDimsAttr(buildI64ArrayAttr(ctx, reductionDims));
  consumer.setPartialReductionOwnerDimsAttr(
      buildI64ArrayAttr(ctx, target.ownerDims));
  return success();
}

struct SdeRedistributePass : public carts::sde::impl::SdeRedistributeBase<SdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    carts::sde::RedistributionEdges committed = carts::sde::collectRedistributionEdges(module);
    bool sawFailure = false;
    for (const auto &failure : committed.failures) {
      carts::sde::SdeSuIterateOp consumer = failure.consumer;
      consumer.emitOpError() << "sde-redistribute: " << failure.reason
          << " (array " << failure.arrayId << "); refusing to invent redistribution";
      sawFailure = true;
    }
    for (const auto &edge : committed.edges) {
      carts::sde::RedistributionEdge emitEdge = edge;
      auto consumer = emitEdge.consumer;
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter) {
        bool sourceEndpointFits =
            endpointShapeFitsRoot(emitEdge.root, emitEdge.sourceOwnerDims,
                                  emitEdge.sourceBlockShape);
        bool shouldTargetWriteResult =
            consumer.getPartialReductionAttr() || !sourceEndpointFits ||
            isElementwisePipeline(consumer);
        FailureOr<std::optional<ReduceScatterWriteTarget>> target =
            shouldTargetWriteResult ? findReduceScatterWriteTarget(consumer)
                                    : std::optional<ReduceScatterWriteTarget>();
        if (failed(target)) {
          consumer.emitOpError()
              << "sde-redistribute: partial-reduction reduce_scatter has "
                 "multiple committed write-result targets; refusing to choose";
          sawFailure = true;
          continue;
        }
        if (shouldTargetWriteResult && !*target) {
          consumer.emitOpError()
              << "sde-redistribute: reduce_scatter source endpoint is not "
                 "representable and no single committed write-result target "
                 "was found";
          sawFailure = true;
          continue;
        }
        if (*target) {
          if (failed(ensurePartialReductionFacts(consumer, **target))) {
            consumer.emitOpError()
                << "sde-redistribute: could not derive partial-reduction "
                   "facts for the committed write-result target";
            sawFailure = true;
            continue;
          }
          emitEdge.root = (*target)->root;
          emitEdge.arrayId = (*target)->arrayId;
          emitEdge.sourceOwnerDims.assign((*target)->ownerDims.begin(),
                                          (*target)->ownerDims.end());
          emitEdge.sourceBlockShape.assign((*target)->blockShape.begin(),
                                           (*target)->blockShape.end());
        }
      }
      if (alreadyRepresented(emitEdge)) continue;
      if (!dyn_cast_or_null<carts::sde::SdeSuDistributeOp>(consumer->getParentOp())) {
        consumer.emitOpError() << "sde-redistribute: movement edge for array " << emitEdge.arrayId
            << " is not inside sde.su_distribute; refusing to emit an unscoped movement op";
        sawFailure = true; continue;
      }
      IntegerAttr arrayIdAttr = IntegerAttr::get(IntegerType::get(ctx, 64), emitEdge.arrayId);
      ArrayAttr ownerDims = buildI64ArrayAttr(ctx, emitEdge.sourceOwnerDims);
      ArrayAttr blockShape = buildI64ArrayAttr(ctx, emitEdge.sourceBlockShape);
      OpBuilder builder(consumer);
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::Halo) {
        carts::sde::SdeSuHaloOp::create(builder, consumer.getLoc(), emitEdge.root, arrayIdAttr,
            ownerDims, blockShape, buildI64ArrayAttr(ctx, emitEdge.haloShape));
        continue;
      }
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter) {
        auto kind = getFirstReductionKind(consumer).value_or(carts::sde::SdeReductionKind::add);
        carts::sde::SdeSuReduceScatterOp::create(builder, consumer.getLoc(), emitEdge.root, arrayIdAttr,
            ownerDims, blockShape, IntegerAttr::get(IntegerType::get(ctx, 64), 0),
            carts::sde::SdeReductionKindAttr::get(ctx, kind));
        continue;
      }
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::AllToAll) {
        carts::sde::SdeSuAllToAllOp::create(
            builder, consumer.getLoc(), emitEdge.root, arrayIdAttr,
            buildI64ArrayAttr(ctx, emitEdge.sourceOwnerDims),
            buildI64ArrayAttr(ctx, emitEdge.sourceBlockShape),
            buildI64ArrayAttr(ctx, emitEdge.targetOwnerDims),
            buildI64ArrayAttr(ctx, emitEdge.targetBlockShape));
        continue;
      }
      consumer.emitOpError() << "sde-redistribute: unexpected redistribution edge kind";
      sawFailure = true;
    }
    if (sawFailure) signalPassFailure();
  }
};
}
namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRedistributePass() {
  return std::make_unique<SdeRedistributePass>();
}
}
