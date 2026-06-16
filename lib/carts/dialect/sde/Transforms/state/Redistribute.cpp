/// Redistribute.cpp
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/RedistributionEdges.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Transforms/effect/distribution/BlockGrainPlan.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "llvm/ADT/DenseMap.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_SDEREDISTRIBUTE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include <memory>
using namespace mlir;
using namespace mlir::carts;
namespace {
static bool alreadyRepresented(const carts::sde::RedistributionEdge &edge) {
  for (Operation *user : edge.root.getUsers()) {
    if (auto halo = dyn_cast<carts::sde::SdeSuHaloOp>(user)) {
      if (edge.kind == carts::sde::RedistributionEdgeKind::Halo &&
          halo.getArrayIdAttr() &&
          halo.getArrayIdAttr().getInt() == edge.arrayId &&
          halo.getOwnerDims() ==
              buildI64ArrayAttr(halo.getContext(), edge.sourceOwnerDims) &&
          halo.getBlockShape() ==
              buildI64ArrayAttr(halo.getContext(), edge.sourceBlockShape) &&
          halo.getHaloShape() ==
              buildI64ArrayAttr(halo.getContext(), edge.haloShape))
        return true;
    }
    if (auto reduce = dyn_cast<carts::sde::SdeSuReduceScatterOp>(user)) {
      if (edge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter &&
          reduce.getArrayIdAttr() &&
          reduce.getArrayIdAttr().getInt() == edge.arrayId &&
          reduce.getOwnerDims() ==
              buildI64ArrayAttr(reduce.getContext(), edge.sourceOwnerDims) &&
          reduce.getBlockShape() ==
              buildI64ArrayAttr(reduce.getContext(), edge.sourceBlockShape))
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
  if (!kinds || kinds.empty())
    return std::nullopt;
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
      ArrayRef<int64_t> blockShape =
          fact.budgetBlockShape.empty()
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

static SmallVector<int64_t, 4> ownerDimsAsI64(ArrayRef<unsigned> ownerDims) {
  SmallVector<int64_t, 4> result;
  result.reserve(ownerDims.size());
  for (unsigned dim : ownerDims)
    result.push_back(static_cast<int64_t>(dim));
  return result;
}

static bool ownerDimsContainAll(ArrayRef<int64_t> superset,
                                ArrayRef<int64_t> subset) {
  for (int64_t dim : subset)
    if (!llvm::is_contained(superset, dim))
      return false;
  return true;
}

static int64_t productOrOne(ArrayRef<int64_t> values) {
  int64_t product = 1;
  for (int64_t value : values) {
    if (value <= 0)
      return 1;
    product *= value;
  }
  return product;
}

static bool rewriteFactToExpandedRoot(Builder &builder, DictionaryAttr dict,
                                      const carts::sde::LayoutGraphFact &fact,
                                      ArrayRef<int64_t> ownerDims,
                                      ArrayRef<int64_t> blockShape,
                                      int64_t muBlockCount,
                                      Attribute &rewritten) {
  StringAttr ownerName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::OwnerDims);
  StringAttr blockShapeName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr muCountName =
      builder.getStringAttr(carts::sde::AttrNames::LayoutGraph::MuBlockCount);

  if (fact.ownerDims == ownerDims && fact.blockShape == blockShape &&
      fact.muBlockCount == muBlockCount)
    return false;

  SmallVector<NamedAttribute, 8> fields;
  for (NamedAttribute named : dict)
    if (named.getName() != ownerName && named.getName() != blockShapeName &&
        named.getName() != muCountName)
      fields.push_back(named);
  MLIRContext *ctx = builder.getContext();
  fields.push_back(
      builder.getNamedAttr(ownerName, buildI64ArrayAttr(ctx, ownerDims)));
  fields.push_back(
      builder.getNamedAttr(blockShapeName, buildI64ArrayAttr(ctx, blockShape)));
  if (muBlockCount > 0)
    fields.push_back(builder.getNamedAttr(
        muCountName, builder.getI64IntegerAttr(muBlockCount)));
  rewritten = builder.getDictionaryAttr(fields);
  return true;
}

// Rank expansion is the real SDE storage transformation. Some nested-stencil
// Jacobi shapes reach redistribution with older layout facts that still name a
// multi-owner logical grid even though the materialized MU is an owner strip.
// Reconcile only facts whose owner set is a strict superset of the recovered
// expanded-root owners (or whose physical block/count is stale) so movement is
// authored over the storage SDE actually built.
static void reconcileLayoutFactsToExpandedRoots(Operation *moduleOp) {
  MLIRContext *ctx = moduleOp->getContext();
  Builder builder(ctx);
  moduleOp->walk([&](carts::sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;

    bool changed = false;
    SmallVector<Attribute, 4> rewritten;
    rewritten.reserve(layout.size());
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<carts::sde::LayoutGraphFact> fact =
          dict ? carts::sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !fact ||
          fact->layoutKind != carts::sde::ArrayLayoutKind::blockParallel ||
          fact->ownerDims.empty() || fact->blockShape.empty()) {
        rewritten.push_back(attr);
        continue;
      }

      carts::sde::SdeAccessMode mode =
          fact->role == carts::sde::LayoutGraphRole::write
              ? carts::sde::SdeAccessMode::write
              : carts::sde::SdeAccessMode::read;
      Value root = carts::sde::findArrayLayoutRoot(op, fact->id, mode);
      auto type = root ? dyn_cast<MemRefType>(root.getType()) : MemRefType();
      // Rank-expanded storage has K grid dims + L tile dims, so rank exceeds
      // the logical blockShape rank. Plain function-arg memrefs are flat and
      // must not be "reconciled" via type-shape guessing.
      if (!type || type.getRank() <= fact->blockShape.size()) {
        rewritten.push_back(attr);
        continue;
      }
      std::optional<carts::sde::ExpandedBlockGridMu> expanded =
          carts::sde::recognizeExpandedBlockGridMu(op, type);
      if (!expanded || expanded->ownerDims.empty()) {
        rewritten.push_back(attr);
        continue;
      }

      SmallVector<int64_t, 4> recoveredOwners =
          ownerDimsAsI64(expanded->ownerDims);
      if (!ownerDimsContainAll(fact->ownerDims, recoveredOwners)) {
        rewritten.push_back(attr);
        continue;
      }

      SmallVector<int64_t, 4> physicalBlockShape(expanded->logicalRank);
      ArrayRef<int64_t> tiles =
          type.getShape().drop_front(expanded->ownerDims.size());
      for (unsigned dim = 0; dim < expanded->logicalRank; ++dim)
        physicalBlockShape[dim] = tiles[dim];

      Attribute updated = attr;
      int64_t recoveredBlocks = productOrOne(expanded->gridCounts);
      if (rewriteFactToExpandedRoot(builder, dict, *fact, recoveredOwners,
                                    physicalBlockShape, recoveredBlocks,
                                    updated)) {
        rewritten.push_back(updated);
        changed = true;
        continue;
      }
      rewritten.push_back(attr);
    }

    if (changed)
      op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
  });
}

static bool ensureMovementScope(carts::sde::SdeSuIterateOp consumer,
                                carts::sde::RedistributionEdgeKind edgeKind) {
  if (consumer->getParentOfType<carts::sde::SdeSuDistributeOp>())
    return true;
  if (consumer.getNumResults() != 0)
    return false;

  carts::sde::SdeDistributionKind kind =
      edgeKind == carts::sde::RedistributionEdgeKind::Halo
          ? carts::sde::SdeDistributionKind::owner_compute
          : carts::sde::SdeDistributionKind::blocked;
  IRRewriter rewriter(consumer.getContext());
  rewriter.setInsertionPoint(consumer);
  auto distribute = carts::sde::SdeSuDistributeOp::create(
      rewriter, consumer.getLoc(),
      carts::sde::SdeDistributionKindAttr::get(consumer.getContext(), kind));
  Block &body = carts::sde::ensureBlock(distribute.getBody());
  consumer->moveBefore(&body, body.end());
  return true;
}

static LogicalResult
ensurePartialReductionFacts(carts::sde::SdeSuIterateOp consumer,
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

// Owner-subset reconciliation for stencil double-buffering. A 2-D-halo stencil
// writes its output on the full owner tile (e.g. [0,1]); an elementwise
// copy-back consumer of that same array commits a coarser single-owner
// row-strip read fact (e.g. [0]) even though RankExpandMu realized the SAME
// rank-expanded grid MU for both (the consumer loads/stores the array on the
// writer's [0,1] grid coordinates). RedistributionEdges then sees DIFFERENT
// owner dims and tries an sde.su_all_to_all whose block extent overflows the
// expanded grid dim. The owner difference is a layout-assignment artifact, not
// a data-location difference: the consumer already lives on the producer grid.
// Re-commit such an owner-subset elementwise reader to the writer home's owner
// layout so producer==consumer and no redistribution edge is emitted. Strictly
// fail-closed: only fires when the reader's grounded root MU type equals the
// writer's, the reader owners are a proper subset of the home owners, and the
// reader carries no halo (a stencil/reduction consumer keeps its own fact).
// Runs after SdeRankExpandMu, which is what materializes the writer's
// multi-owner physical write fact and the shared expanded root type.
static void reconcileSubsetOwnerExpandedGridReaders(Operation *moduleOp) {
  struct WriterHome {
    sde::SdeSuIterateOp writer;
    SmallVector<int64_t, 4> ownerDims;
    SmallVector<int64_t, 4> blockShape;
    SmallVector<int64_t, 4> budgetShape;
    int64_t muBlockCount = 1;
    bool conflicting = false;
  };
  llvm::DenseMap<int64_t, WriterHome> homeById;

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const sde::LayoutGraphFact &f : sde::parseArrayLayoutFacts(layout)) {
      if (f.id < 0 || f.role != sde::LayoutGraphRole::write ||
          f.layoutKind != sde::ArrayLayoutKind::blockParallel ||
          f.ownerDims.size() < 2)
        continue;
      WriterHome &home = homeById[f.id];
      if (home.writer) {
        // Two writers that commit the SAME block-distributed grain (e.g. an
        // elementwise init writer and the stencil that both author
        // [0,1]/[512,512] for the same DB) are not a conflict: they describe
        // one single-writer home. Only genuinely divergent owner/block grain
        // disables reader reconciliation.
        bool sameGrain =
            home.ownerDims.size() == f.ownerDims.size() &&
            std::equal(home.ownerDims.begin(), home.ownerDims.end(),
                       f.ownerDims.begin()) &&
            home.blockShape.size() == f.blockShape.size() &&
            std::equal(home.blockShape.begin(), home.blockShape.end(),
                       f.blockShape.begin());
        if (!sameGrain)
          home.conflicting = true;
        continue;
      }
      home.writer = op;
      home.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
      home.blockShape.assign(f.blockShape.begin(), f.blockShape.end());
      home.budgetShape.assign(f.budgetBlockShape.begin(),
                              f.budgetBlockShape.end());
      home.muBlockCount = f.muBlockCount;
    }
  });
  if (homeById.empty())
    return;

  auto sameOwnerSet = [](ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
    if (lhs.size() != rhs.size())
      return false;
    SmallVector<int64_t, 4> a(lhs.begin(), lhs.end());
    SmallVector<int64_t, 4> b(rhs.begin(), rhs.end());
    llvm::sort(a);
    llvm::sort(b);
    return a == b;
  };
  auto isProperOwnerSubset = [](ArrayRef<int64_t> sub, ArrayRef<int64_t> sup) {
    if (sub.empty() || sub.size() >= sup.size())
      return false;
    for (int64_t d : sub)
      if (!llvm::is_contained(sup, d))
        return false;
    return true;
  };

  MLIRContext *ctx = moduleOp->getContext();
  Builder builder(ctx);
  StringAttr ownerName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::OwnerDims);
  StringAttr blockShapeName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr budgetName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BudgetBlockShape);
  StringAttr muCountName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::MuBlockCount);

  moduleOp->walk([&](sde::SdeSuIterateOp reader) {
    ArrayAttr layout = reader.getArrayLayoutAttr();
    if (!layout)
      return;
    // Stencil/reduction consumers own their coarser fact; never coarsen the
    // producer grid out from under a halo or contraction read.
    if (auto cls = sde::queryStructuredClassification(reader);
        cls && (*cls == sde::SdeStructuredClassification::stencil ||
                *cls == sde::SdeStructuredClassification::matmul))
      return;
    if (sde::deriveCommittedHaloShape(reader))
      return;

    bool changed = false;
    SmallVector<Attribute, 4> rewritten;
    rewritten.reserve(layout.size());
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> f =
          dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !f || f->role != sde::LayoutGraphRole::read ||
          f->layoutKind != sde::ArrayLayoutKind::blockParallel) {
        rewritten.push_back(attr);
        continue;
      }
      auto it = homeById.find(f->id);
      if (it == homeById.end() || it->second.conflicting ||
          it->second.writer == reader ||
          !isProperOwnerSubset(f->ownerDims, it->second.ownerDims)) {
        rewritten.push_back(attr);
        continue;
      }
      const WriterHome &home = it->second;
      // The reader must already physically live on the writer's rank-expanded
      // grid: same grounded root MU type. Otherwise the owner difference is a
      // real data-location difference and the all_to_all/fail-closed path owns
      // it.
      Value readerRoot =
          sde::findArrayLayoutRoot(reader, f->id, sde::SdeAccessMode::read);
      Value writerRoot = sde::findArrayLayoutRoot(home.writer, f->id,
                                                  sde::SdeAccessMode::write);
      if (!readerRoot || !writerRoot ||
          readerRoot.getType() != writerRoot.getType()) {
        rewritten.push_back(attr);
        continue;
      }
      if (sameOwnerSet(f->ownerDims, home.ownerDims)) {
        rewritten.push_back(attr);
        continue;
      }
      SmallVector<NamedAttribute, 8> fields;
      for (NamedAttribute named : dict)
        if (named.getName() != ownerName && named.getName() != blockShapeName &&
            named.getName() != budgetName && named.getName() != muCountName)
          fields.push_back(named);
      fields.push_back(builder.getNamedAttr(
          ownerName, buildI64ArrayAttr(ctx, home.ownerDims)));
      fields.push_back(builder.getNamedAttr(
          blockShapeName, buildI64ArrayAttr(ctx, home.blockShape)));
      if (!home.budgetShape.empty())
        fields.push_back(builder.getNamedAttr(
            budgetName, buildI64ArrayAttr(ctx, home.budgetShape)));
      if (home.muBlockCount > 0)
        fields.push_back(builder.getNamedAttr(
            muCountName, builder.getI64IntegerAttr(home.muBlockCount)));
      rewritten.push_back(builder.getDictionaryAttr(fields));
      changed = true;
    }
    if (changed)
      reader.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
  });
}

static bool sameOwnerDimPositions(ArrayRef<int64_t> lhs,
                                  ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

static ArrayRef<int64_t> committedBlockGrain(const sde::LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty()
             ? ArrayRef<int64_t>(fact.blockShape)
             : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static int64_t blockGrainVolume(ArrayRef<int64_t> shape) {
  int64_t volume = 1;
  for (int64_t dim : shape)
    if (dim > 0)
      volume *= dim;
  return volume;
}

static bool isFinerBlockGrain(const sde::LayoutGraphFact &candidate,
                              const sde::LayoutGraphFact &baseline) {
  ArrayRef<int64_t> candidateGrain = committedBlockGrain(candidate);
  ArrayRef<int64_t> baselineGrain = committedBlockGrain(baseline);
  if (candidateGrain.empty() || baselineGrain.empty() ||
      candidateGrain.size() != baselineGrain.size())
    return candidate.muBlockCount > baseline.muBlockCount;
  int64_t candidateVolume = blockGrainVolume(candidateGrain);
  int64_t baselineVolume = blockGrainVolume(baselineGrain);
  if (candidateVolume != baselineVolume)
    return candidateVolume < baselineVolume;
  return candidate.muBlockCount > baseline.muBlockCount;
}

static void restateArrayLayoutFactGrain(Builder &builder, DictionaryAttr dict,
                                        ArrayRef<int64_t> ownerDims,
                                        ArrayRef<int64_t> blockShape,
                                        ArrayRef<int64_t> budgetBlockShape,
                                        int64_t muBlockCount, Attribute &out) {
  MLIRContext *ctx = builder.getContext();
  StringAttr blockShapeName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr budgetName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BudgetBlockShape);
  StringAttr muCountName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::MuBlockCount);
  StringAttr ownerName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::OwnerDims);

  SmallVector<NamedAttribute, 8> fields;
  for (NamedAttribute named : dict)
    if (named.getName() != ownerName && named.getName() != blockShapeName &&
        named.getName() != budgetName && named.getName() != muCountName)
      fields.push_back(named);
  fields.push_back(
      builder.getNamedAttr(ownerName, buildI64ArrayAttr(ctx, ownerDims)));
  fields.push_back(
      builder.getNamedAttr(blockShapeName, buildI64ArrayAttr(ctx, blockShape)));
  if (!budgetBlockShape.empty())
    fields.push_back(builder.getNamedAttr(
        budgetName, buildI64ArrayAttr(ctx, budgetBlockShape)));
  if (muBlockCount > 0)
    fields.push_back(builder.getNamedAttr(
        muCountName, builder.getI64IntegerAttr(muBlockCount)));
  out = builder.getDictionaryAttr(fields);
}

static bool sameI64Array(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

static void propagateArrayBlockGrainOnSu(sde::SdeSuIterateOp op,
                                         int64_t arrayId,
                                         ArrayRef<int64_t> ownerDims,
                                         ArrayRef<int64_t> blockShape,
                                         ArrayRef<int64_t> budgetBlockShape,
                                         int64_t muBlockCount) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return;
  MLIRContext *ctx = op.getContext();
  Builder builder(ctx);
  bool changed = false;
  SmallVector<Attribute, 4> rewritten;
  rewritten.reserve(layout.size());
  for (Attribute attr : layout) {
    auto dict = dyn_cast<DictionaryAttr>(attr);
    std::optional<sde::LayoutGraphFact> fact =
        dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
    if (!dict || !fact || fact->id != arrayId ||
        fact->layoutKind != sde::ArrayLayoutKind::blockParallel ||
        !sameOwnerDimPositions(fact->ownerDims, ownerDims) ||
        (sameI64Array(fact->blockShape, blockShape) &&
         sameI64Array(fact->budgetBlockShape, budgetBlockShape) &&
         (!muBlockCount || fact->muBlockCount == muBlockCount))) {
      rewritten.push_back(attr);
      continue;
    }
    Attribute updated = attr;
    restateArrayLayoutFactGrain(builder, dict, ownerDims, blockShape,
                                budgetBlockShape, muBlockCount, updated);
    rewritten.push_back(updated);
    changed = true;
  }
  if (changed)
    op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
}

static std::optional<sde::LayoutGraphFact>
findFinestCommittedFactForArray(Operation *moduleOp, int64_t arrayId) {
  std::optional<sde::LayoutGraphFact> finest;
  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const sde::LayoutGraphFact &fact :
         sde::parseArrayLayoutFacts(layout)) {
      if (fact.id != arrayId || fact.ownerDims.empty() ||
          fact.layoutKind != sde::ArrayLayoutKind::blockParallel)
        continue;
      if (!finest || isFinerBlockGrain(fact, *finest))
        finest = fact;
    }
  });
  return finest;
}

static std::optional<sde::LayoutGraphFact>
lookupSuLayoutFact(sde::SdeSuIterateOp op, int64_t arrayId,
                   std::optional<sde::LayoutGraphRole> role = std::nullopt) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;
  std::optional<sde::LayoutGraphFact> match;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.id != arrayId || fact.ownerDims.empty() ||
        fact.layoutKind != sde::ArrayLayoutKind::blockParallel)
      continue;
    if (role && fact.role != *role)
      continue;
    if (!match || isFinerBlockGrain(fact, *match))
      match = fact;
  }
  return match;
}

static DictionaryAttr findFinestFactDictForArray(Operation *moduleOp,
                                                 int64_t arrayId) {
  std::optional<sde::LayoutGraphFact> finest;
  DictionaryAttr finestDict;
  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> fact =
          dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !fact || fact->id != arrayId || fact->ownerDims.empty() ||
          fact->layoutKind != sde::ArrayLayoutKind::blockParallel)
        continue;
      if (!finest || isFinerBlockGrain(*fact, *finest)) {
        finest = *fact;
        finestDict = dict;
      }
    }
  });
  return finestDict;
}

static void ensureWriteFactForArrayRoot(sde::SdeSuIterateOp op, int64_t arrayId,
                                        DictionaryAttr homeDict) {
  if (!homeDict || lookupSuLayoutFact(op, arrayId, sde::LayoutGraphRole::write))
    return;
  MLIRContext *ctx = op.getContext();
  Builder builder(ctx);
  StringAttr roleName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::Role);
  StringAttr writeRole =
      builder.getStringAttr(sde::AttrNames::LayoutGraphValues::RoleWrite);

  SmallVector<Attribute, 4> facts;
  if (ArrayAttr layout = op.getArrayLayoutAttr())
    facts.assign(layout.begin(), layout.end());

  SmallVector<NamedAttribute, 8> fields;
  for (NamedAttribute named : homeDict)
    if (named.getName() != roleName)
      fields.push_back(named);
  fields.push_back(builder.getNamedAttr(roleName, writeRole));
  facts.push_back(builder.getDictionaryAttr(fields));
  op.setArrayLayoutAttr(ArrayAttr::get(ctx, facts));
}

static LogicalResult
rematerializeExpandedMuForArray(Operation *moduleOp, int64_t arrayId,
                                ArrayRef<int64_t> ownerDims,
                                ArrayRef<int64_t> blockShape) {
  sde::SdeMuAllocOp targetMu;
  moduleOp->walk([&](sde::SdeMuAllocOp mu) {
    if (targetMu)
      return;
    std::optional<int64_t> muArrayId = sde::getMuArrayIdFromLayoutRoot(mu);
    if (muArrayId && *muArrayId == arrayId)
      targetMu = mu;
  });
  if (!targetMu)
    return success();

  auto expandedType = dyn_cast<MemRefType>(targetMu.getMemref().getType());
  if (!expandedType || !expandedType.hasStaticShape())
    return success();
  std::optional<sde::RecoveredMuPhysicalLayout> recovered =
      sde::recoverMuPhysicalLayoutFromExpandedType(expandedType);
  if (!recovered || recovered->ownerDims.empty())
    return success();

  const unsigned numGrid = recovered->ownerDims.size();
  SmallVector<int64_t, 4> newShape(expandedType.getShape().begin(),
                                   expandedType.getShape().end());
  llvm::SmallDenseMap<int64_t, int64_t, 4> oldToNewBlockExtent;
  for (auto [slot, od] : llvm::enumerate(recovered->ownerDims)) {
    if (slot >= ownerDims.size() || ownerDims[slot] != static_cast<int64_t>(od))
      return success();
    int64_t oldBlock = recovered->physicalBlockShape[od];
    int64_t newBlock =
        blockShape.size() == recovered->logicalShape.size()
            ? blockShape[od]
            : (slot < blockShape.size() ? blockShape[slot] : oldBlock);
    if (newBlock <= 0 || newBlock >= oldBlock)
      continue;
    if (oldBlock % newBlock != 0)
      return failure();
    int64_t scale = oldBlock / newBlock;
    newShape[slot] *= scale;
    newShape[numGrid + od] = newBlock;
    oldToNewBlockExtent[oldBlock] = newBlock;
  }
  if (oldToNewBlockExtent.empty())
    return success();

  MemRefType retiledType =
      MemRefType::get(newShape, expandedType.getElementType());
  if (retiledType == expandedType)
    return success();

  Value muValue = targetMu.getMemref();
  MLIRContext *ctx = moduleOp->getContext();
  OpBuilder builder(ctx);
  auto usesBlockConstant = [&](Value index, int64_t oldBlock) -> bool {
    Operation *def = index.getDefiningOp();
    if (!def)
      return false;
    Value rhs;
    if (auto div = dyn_cast<arith::DivUIOp>(def))
      rhs = div.getRhs();
    else if (auto rem = dyn_cast<arith::RemUIOp>(def))
      rhs = rem.getRhs();
    else
      return false;
    auto cst = rhs.getDefiningOp<arith::ConstantOp>();
    if (!cst)
      return false;
    auto intAttr = dyn_cast<IntegerAttr>(cst.getValue());
    return intAttr && intAttr.getValue().isNonNegative() &&
           intAttr.getValue().getZExtValue() == static_cast<uint64_t>(oldBlock);
  };

  auto rewriteIndexing = [&](Operation *op) {
    auto retileIndex = [&](Location loc, Value index) -> Value {
      for (const auto &kv : oldToNewBlockExtent) {
        if (!usesBlockConstant(index, kv.first))
          continue;
        Operation *def = index.getDefiningOp();
        if (!def)
          continue;
        Value lhs = isa<arith::DivUIOp>(def)
                        ? cast<arith::DivUIOp>(def).getLhs()
                        : cast<arith::RemUIOp>(def).getLhs();
        auto newCst = arith::ConstantIndexOp::create(builder, loc, kv.second);
        if (isa<arith::DivUIOp>(def))
          return arith::DivUIOp::create(builder, loc, lhs, newCst).getResult();
        return arith::RemUIOp::create(builder, loc, lhs, newCst).getResult();
      }
      return index;
    };
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (store.getMemref() != muValue)
        return;
      SmallVector<Value, 4> newIndices;
      bool changed = false;
      for (Value index : store.getIndices()) {
        Value retiled = retileIndex(store.getLoc(), index);
        newIndices.push_back(retiled);
        changed |= (retiled != index);
      }
      if (changed)
        store.getIndicesMutable().assign(newIndices);
      return;
    }
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (load.getMemref() != muValue)
        return;
      SmallVector<Value, 4> newIndices;
      bool changed = false;
      for (Value index : load.getIndices()) {
        Value retiled = retileIndex(load.getLoc(), index);
        newIndices.push_back(retiled);
        changed |= (retiled != index);
      }
      if (changed)
        load.getIndicesMutable().assign(newIndices);
    }
  };

  moduleOp->walk(rewriteIndexing);
  targetMu.getMemref().setType(retiledType);
  return success();
}

static bool isAlternatingBufferShapePair(ArrayRef<int64_t> read,
                                         ArrayRef<int64_t> write) {
  if (read.size() != write.size() || read.size() < 2)
    return false;
  for (size_t i = 1; i < read.size(); ++i)
    if (read[i] != write[i])
      return false;
  return read[0] > 0 && write[0] > 0 && read[0] != write[0] &&
         (write[0] % read[0] == 0 || read[0] % write[0] == 0);
}

static void unifyAlternatingBufferPair(Operation *moduleOp,
                                       sde::SdeSuIterateOp op,
                                       const sde::LayoutGraphFact &readFact,
                                       const sde::LayoutGraphFact &writeFact) {
  if (readFact.id == writeFact.id ||
      !sameOwnerDimPositions(readFact.ownerDims, writeFact.ownerDims) ||
      !isAlternatingBufferShapePair(readFact.blockShape, writeFact.blockShape))
    return;
  if (sameI64Array(readFact.blockShape, writeFact.blockShape))
    return;

  if (DictionaryAttr homeDict =
          findFinestFactDictForArray(moduleOp, writeFact.id))
    ensureWriteFactForArrayRoot(op, writeFact.id, homeDict);

  propagateArrayBlockGrainOnSu(op, writeFact.id, writeFact.ownerDims,
                               readFact.blockShape, readFact.budgetBlockShape,
                               readFact.muBlockCount);
  (void)sde::rewriteWriterArrayLayoutToPhysicalShape(
      op, writeFact.ownerDims, readFact.blockShape, writeFact.id);
  // Physical MU retile is applied in a follow-up walk after all pairs are
  // unified so halo/movement lowering sees one block table.
}

// Jacobi/poisson double-buffer copies read `unew` and write `u` on the same
// owner dim but can commit different block grains (e.g. 4x256 vs 2x512 rows).
// ARTS compact-halo packing and the copy-back EDT assume matching block tables;
// unify the write buffer to the read source grain before edge collection.
static void reconcileAlternatingBufferGrain(Operation *moduleOp) {
  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    std::optional<sde::LayoutGraphFact> readFact;
    std::optional<sde::LayoutGraphFact> writeFact;
    for (const sde::LayoutGraphFact &fact :
         sde::parseArrayLayoutFacts(layout)) {
      if (fact.id < 0 || fact.ownerDims.empty() ||
          fact.layoutKind != sde::ArrayLayoutKind::blockParallel)
        continue;
      if (fact.role == sde::LayoutGraphRole::read) {
        if (!readFact || isFinerBlockGrain(fact, *readFact))
          readFact = fact;
      } else if (fact.role == sde::LayoutGraphRole::write) {
        if (!writeFact || isFinerBlockGrain(*writeFact, fact))
          writeFact = fact;
      }
    }
    if (!readFact || !writeFact)
      return;
    unifyAlternatingBufferPair(moduleOp, op, *readFact, *writeFact);
  });

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    if (op.getBody().empty())
      return;
    std::optional<int64_t> readArrayId;
    std::optional<int64_t> writeArrayId;
    for (sde::SdeArrayLayoutRootOp root :
         op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
      if (!root.getArrayIdAttr())
        continue;
      int64_t id = root.getArrayIdAttr().getInt();
      if (root.getMode() == sde::SdeAccessMode::read)
        readArrayId = id;
      else if (root.getMode() == sde::SdeAccessMode::write)
        writeArrayId = id;
    }
    if (!readArrayId || !writeArrayId || *readArrayId == *writeArrayId)
      return;

    std::optional<sde::LayoutGraphFact> readFact =
        lookupSuLayoutFact(op, *readArrayId, sde::LayoutGraphRole::read);
    if (!readFact)
      readFact = findFinestCommittedFactForArray(moduleOp, *readArrayId);
    std::optional<sde::LayoutGraphFact> writeFact =
        lookupSuLayoutFact(op, *writeArrayId, sde::LayoutGraphRole::write);
    if (!writeFact)
      writeFact = findFinestCommittedFactForArray(moduleOp, *writeArrayId);
    if (!readFact || !writeFact)
      return;
    unifyAlternatingBufferPair(moduleOp, op, *readFact, *writeFact);
  });

  llvm::SmallDenseSet<int64_t, 4> retiledArrays;
  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    if (op.getBody().empty())
      return;
    std::optional<int64_t> readArrayId;
    std::optional<int64_t> writeArrayId;
    for (sde::SdeArrayLayoutRootOp root :
         op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>()) {
      if (!root.getArrayIdAttr())
        continue;
      int64_t id = root.getArrayIdAttr().getInt();
      if (root.getMode() == sde::SdeAccessMode::read)
        readArrayId = id;
      else if (root.getMode() == sde::SdeAccessMode::write)
        writeArrayId = id;
    }
    if (!readArrayId || !writeArrayId || *readArrayId == *writeArrayId)
      return;
    std::optional<sde::LayoutGraphFact> readFact =
        lookupSuLayoutFact(op, *readArrayId, sde::LayoutGraphRole::read);
    if (!readFact)
      readFact = findFinestCommittedFactForArray(moduleOp, *readArrayId);
    std::optional<sde::LayoutGraphFact> writeFact =
        lookupSuLayoutFact(op, *writeArrayId, sde::LayoutGraphRole::write);
    if (!writeFact)
      writeFact = findFinestCommittedFactForArray(moduleOp, *writeArrayId);
    if (!readFact || !writeFact || readFact->id == writeFact->id ||
        !isAlternatingBufferShapePair(readFact->blockShape,
                                      writeFact->blockShape) ||
        sameI64Array(readFact->blockShape, writeFact->blockShape))
      return;
    if (retiledArrays.insert(writeFact->id).second)
      (void)rematerializeExpandedMuForArray(
          moduleOp, writeFact->id, writeFact->ownerDims, readFact->blockShape);
  });
}

// Author the committed write access-window dependency for an elementwise INIT
// writer of block-distributed arrays. A data-parallel init loop (e.g.
// `u[i][j] = 0; unew[i][j] = 0; f[i][j] = ...`) writes several arrays in one
// full-grid scheduling unit. LayoutAssignment never saw it (the init nest is
// raised to an SU only after Tiling, post-LayoutAssignment), so it carries no
// `arrayLayout` write facts and no `array_layout_root` ops. RankExpandMu still
// rewrote its stores onto the realized block-distributed `sde.mu_alloc` grid
// (div/rem indices), so the init physically writes the SAME committed DBs the
// stencil/copy-back consumers already bind. Without committed write facts,
// `sde-to-arts` fails closed ("touches a DB without a committed SDE
// access-window dependency").
//
// This authors the missing facts by COPYING each consumer's committed home
// layout for the array the init store targets and flipping its role to write.
// It never invents a grain: it fires only when the init's store root is the
// exact `sde.mu_alloc` a consumer binds via `array_layout_root`, so the owner
// dims, block shape, budget grain, and mu-block count are the array's already
// committed DB grain. Strictly fail-closed: if ANY external store in the
// candidate SU targets a root that is not a consumer-bound committed-block DB,
// the SU is left untouched and the boundary keeps rejecting it.
//
// Runs after SdeRankExpandMu (stores already on the expanded grid) and before
// RedistributionEdges, so the init writer and its consumers carry identical
// committed facts and no spurious redistribution edge is emitted between them.
static void authorInitWriterAccessWindows(Operation *moduleOp) {
  // Map each committed block-distributed DB (its mu_alloc) to the home layout
  // fact + array id a consumer SU binds for it. Prefer a write fact; fall back
  // to a read fact (a sole-writer init's only consumer may be a reader).
  struct DbHome {
    int64_t arrayId = -1;
    DictionaryAttr fact; // the consumer's committed fact dict
    bool fromWrite = false;
  };
  llvm::DenseMap<Operation *, DbHome> homeByAlloc;

  auto recordHome = [&](Operation *allocOp, int64_t arrayId,
                        DictionaryAttr fact, bool fromWrite) {
    if (!allocOp || arrayId < 0 || !fact)
      return;
    DbHome &home = homeByAlloc[allocOp];
    // A write fact is the authoritative single-writer grain; never let a read
    // fact overwrite it.
    if (home.fact && home.fromWrite && !fromWrite)
      return;
    home.arrayId = arrayId;
    home.fact = fact;
    home.fromWrite = fromWrite;
  };

  moduleOp->walk([&](sde::SdeSuIterateOp consumer) {
    ArrayAttr layout = consumer.getArrayLayoutAttr();
    if (!layout)
      return;
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> f =
          dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !f || f->id < 0 ||
          f->layoutKind != sde::ArrayLayoutKind::blockParallel ||
          f->ownerDims.empty())
        continue;
      bool isWrite = f->role == sde::LayoutGraphRole::write;
      sde::SdeAccessMode mode =
          isWrite ? sde::SdeAccessMode::write : sde::SdeAccessMode::read;
      Value root = sde::findArrayLayoutRoot(consumer, f->id, mode);
      if (!root)
        continue;
      auto alloc = root.getDefiningOp<sde::SdeMuAllocOp>();
      if (!alloc)
        continue;
      recordHome(alloc.getOperation(), f->id, dict, isWrite);
    }
  });
  if (homeByAlloc.empty())
    return;

  MLIRContext *ctx = moduleOp->getContext();
  Builder builder(ctx);
  StringAttr roleName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::Role);
  StringAttr writeRole =
      builder.getStringAttr(sde::AttrNames::LayoutGraphValues::RoleWrite);

  SmallVector<sde::SdeSuIterateOp, 4> candidates;
  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    // The init writer carries no committed layout and binds no roots yet.
    if (op.getArrayLayoutAttr())
      return;
    if (!op.getBody().empty() &&
        !op.getBody().front().getOps<sde::SdeArrayLayoutRootOp>().empty())
      return;
    candidates.push_back(op);
  });

  for (sde::SdeSuIterateOp op : candidates) {
    if (op.getBody().empty())
      continue;
    // Collect distinct external store roots and verify EVERY one is a
    // consumer-bound committed-block DB. Fail closed otherwise: a single
    // unknown store means we cannot author a complete, correct window set.
    llvm::MapVector<Operation *, DbHome> writes;
    bool sawExternalStore = false;
    bool failClosed = false;
    op.getBody().walk([&](memref::StoreOp store) {
      if (failClosed)
        return;
      Value root = ValueAnalysis::stripMemrefViewOps(store.getMemref());
      auto alloc = root ? root.getDefiningOp<sde::SdeMuAllocOp>() : nullptr;
      if (!alloc) {
        // A store into an SU-local scratch memref is fine; only external
        // (non-mu_alloc) stores that are not block DBs would be unsafe, but
        // those are exactly the ones a coarse SU owns. Treat any non-mu_alloc
        // store root that is defined OUTSIDE the SU as a hard stop.
        if (root && !sde::isDefinedInside(op.getOperation(), root))
          failClosed = true;
        return;
      }
      auto it = homeByAlloc.find(alloc.getOperation());
      if (it == homeByAlloc.end()) {
        failClosed = true;
        return;
      }
      sawExternalStore = true;
      writes.insert({alloc.getOperation(), it->second});
    });
    if (failClosed || !sawExternalStore || writes.empty())
      continue;

    // Fire only for a MULTI-OUTPUT data-parallel init writer: one scheduling
    // unit that writes two or more distinct block DBs over the full grid
    // (jacobi/poisson `f=...; u=0; unew=0;`). Single-output init loops (e.g.
    // each conv/matmul per-array initializer, which is its own SU) already
    // lower through the existing direct boundary path at base; authoring facts
    // for them restates committed owner-dim facts and regresses those kernels.
    // The multi-output init is the case LayoutAssignment structurally misses
    // (the combined nest is raised to one SU only after Tiling), leaving it
    // with no committed window while its stencil consumer commits the grid.
    if (writes.size() < 2)
      continue;

    // And the written DBs must either carry MIXED owner-dim ranks (e.g.
    // jacobi/poisson `f` on [0] plus `u`/`unew` on [0,1]) or share a uniform
    // single-owner rank. The uniform single-owner case covers alternating
    // buffer Jacobi where both initialized DB homes must be committed before
    // redistribution can author the stencil read halos. Uniform multi-owner
    // inits (e.g. conv input+output pairs on [0,1,2]) still auto-derive their
    // boundary windows from store indices at base and must not be disturbed.
    bool anyMultiOwner = false;
    llvm::SmallDenseSet<size_t, 4> ownerRanks;
    for (const auto &kv : writes) {
      std::optional<sde::LayoutGraphFact> f =
          sde::parseArrayLayoutFact(kv.second.fact);
      if (!f)
        continue;
      ownerRanks.insert(f->ownerDims.size());
      if (f->ownerDims.size() >= 2)
        anyMultiOwner = true;
    }
    bool uniformSingleOwner =
        ownerRanks.size() == 1 && !anyMultiOwner && ownerRanks.contains(1);
    bool mixedOwnerRanks = anyMultiOwner && ownerRanks.size() >= 2;
    if (!uniformSingleOwner && !mixedOwnerRanks)
      continue;

    // Author one write fact (copied from the array's committed home, role
    // flipped to write) plus its array_layout_root, per distinct DB. When
    // several outputs share the same owner dim (jacobi/poisson `u` + `unew`),
    // commit the finest shared block grain so the multi-writer init and later
    // copy-back/halo lowering see one block table.
    llvm::SmallVector<std::pair<SmallVector<int64_t, 4>, sde::LayoutGraphFact>,
                      4>
        canonicalByOwnerDims;
    for (const auto &kv : writes) {
      std::optional<sde::LayoutGraphFact> fact =
          sde::parseArrayLayoutFact(kv.second.fact);
      if (!fact || fact->ownerDims.empty() || fact->blockShape.empty())
        continue;
      auto it = llvm::find_if(canonicalByOwnerDims, [&](const auto &entry) {
        return sameOwnerDimPositions(entry.first, fact->ownerDims);
      });
      if (it == canonicalByOwnerDims.end()) {
        canonicalByOwnerDims.push_back({fact->ownerDims, *fact});
        continue;
      }
      if (isFinerBlockGrain(*fact, it->second))
        it->second = *fact;
    }

    SmallVector<Attribute, 4> facts;
    SmallVector<std::pair<Value, DbHome>, 4> roots;
    for (const auto &kv : writes) {
      auto alloc = cast<sde::SdeMuAllocOp>(kv.first);
      const DbHome &home = kv.second;
      std::optional<sde::LayoutGraphFact> sourceFact =
          sde::parseArrayLayoutFact(home.fact);
      if (!sourceFact)
        continue;
      const sde::LayoutGraphFact *canonical = &*sourceFact;
      if (auto it = llvm::find_if(canonicalByOwnerDims,
                                  [&](const auto &entry) {
                                    return sameOwnerDimPositions(
                                        entry.first, sourceFact->ownerDims);
                                  });
          it != canonicalByOwnerDims.end())
        canonical = &it->second;

      SmallVector<NamedAttribute, 8> fields;
      for (NamedAttribute named : home.fact)
        if (named.getName() != roleName)
          fields.push_back(named);
      fields.push_back(builder.getNamedAttr(roleName, writeRole));
      Attribute authored = builder.getDictionaryAttr(fields);
      restateArrayLayoutFactGrain(builder, cast<DictionaryAttr>(authored),
                                  canonical->ownerDims, canonical->blockShape,
                                  canonical->budgetBlockShape,
                                  canonical->muBlockCount, authored);
      facts.push_back(authored);
      roots.push_back({alloc.getMemref(), home});
    }

    op.setArrayLayoutAttr(ArrayAttr::get(ctx, facts));
    OpBuilder rootBuilder(&op.getBody().front(), op.getBody().front().begin());
    for (const auto &rk : roots) {
      sde::SdeArrayLayoutRootOp::create(
          rootBuilder, op.getLoc(), rk.first,
          sde::SdeAccessModeAttr::get(ctx, sde::SdeAccessMode::write),
          IntegerAttr::get(IntegerType::get(ctx, 64), rk.second.arrayId));
    }
    for (const auto &kv : writes) {
      std::optional<sde::LayoutGraphFact> sourceFact =
          sde::parseArrayLayoutFact(kv.second.fact);
      if (!sourceFact)
        continue;
      auto it = llvm::find_if(canonicalByOwnerDims, [&](const auto &entry) {
        return sameOwnerDimPositions(entry.first, sourceFact->ownerDims);
      });
      if (it == canonicalByOwnerDims.end() ||
          committedBlockGrain(*sourceFact) == committedBlockGrain(it->second))
        continue;
      (void)sde::rewriteWriterArrayLayoutToPhysicalShape(
          op, it->second.ownerDims, it->second.blockShape, sourceFact->id);
    }
    // Commit CU group block counts so the compute CU groups the same DB blocks
    // the home consumer does (parsed from the copied home fact).
    for (const auto &kv : writes) {
      std::optional<sde::LayoutGraphFact> f =
          sde::parseArrayLayoutFact(kv.second.fact);
      if (f && !f->ownerDims.empty() && !f->blockShape.empty()) {
        auto it = llvm::find_if(canonicalByOwnerDims, [&](const auto &entry) {
          return sameOwnerDimPositions(entry.first, f->ownerDims);
        });
        if (it != canonicalByOwnerDims.end())
          sde::commitCuGroupBlockCounts(op, it->second.ownerDims,
                                        it->second.blockShape);
        else
          sde::commitCuGroupBlockCounts(op, f->ownerDims, f->blockShape);
        break;
      }
    }
  }
}

struct SdeRedistributePass
    : public carts::sde::impl::SdeRedistributeBase<SdeRedistributePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    authorInitWriterAccessWindows(module);
    reconcileSubsetOwnerExpandedGridReaders(module);
    reconcileLayoutFactsToExpandedRoots(module);
    // RankExpandMu may expose new budget/physical grains after BlockGrainPlan;
    // re-unify same-owner block grain so home==reader before edge collection.
    reconcileAlternatingBufferGrain(module);
    carts::sde::distribution::reconcileSameOwnerArrayGrain(
        module, carts::sde::distribution::SameOwnerGrainUnifyKind::
                    CoarseCompatiblePostExpand);
    reconcileAlternatingBufferGrain(module);
    carts::sde::RedistributionEdges committed =
        carts::sde::collectRedistributionEdges(module);
    bool sawFailure = false;
    for (const auto &failure : committed.failures) {
      carts::sde::SdeSuIterateOp consumer = failure.consumer;
      consumer.emitOpError()
          << "sde-redistribute: " << failure.reason << " (array "
          << failure.arrayId << "); refusing to invent redistribution";
      sawFailure = true;
    }
    for (const auto &edge : committed.edges) {
      carts::sde::RedistributionEdge emitEdge = edge;
      auto consumer = emitEdge.consumer;
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter) {
        bool sourceEndpointFits = endpointShapeFitsRoot(
            emitEdge.root, emitEdge.sourceOwnerDims, emitEdge.sourceBlockShape);
        bool shouldTargetWriteResult = consumer.getPartialReductionAttr() ||
                                       !sourceEndpointFits ||
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
      if (alreadyRepresented(emitEdge))
        continue;
      (void)ensureMovementScope(consumer, emitEdge.kind);
      if (!dyn_cast_or_null<carts::sde::SdeSuDistributeOp>(
              consumer->getParentOp())) {
        consumer.emitOpError()
            << "sde-redistribute: movement edge for array " << emitEdge.arrayId
            << " is not inside sde.su_distribute; refusing to emit an unscoped "
               "movement op";
        sawFailure = true;
        continue;
      }
      IntegerAttr arrayIdAttr =
          IntegerAttr::get(IntegerType::get(ctx, 64), emitEdge.arrayId);
      ArrayAttr ownerDims = buildI64ArrayAttr(ctx, emitEdge.sourceOwnerDims);
      ArrayAttr blockShape = buildI64ArrayAttr(ctx, emitEdge.sourceBlockShape);
      OpBuilder builder(consumer);
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::Halo) {
        if (auto cu = carts::sde::findSuComputeCuRegion(consumer))
          cu.removeGroupBlockCountAttr();
        carts::sde::SdeSuHaloOp::create(
            builder, consumer.getLoc(), emitEdge.root, arrayIdAttr, ownerDims,
            blockShape, buildI64ArrayAttr(ctx, emitEdge.haloShape));
        continue;
      }
      if (emitEdge.kind == carts::sde::RedistributionEdgeKind::ReduceScatter) {
        auto kind = getFirstReductionKind(consumer).value_or(
            carts::sde::SdeReductionKind::add);
        carts::sde::SdeSuReduceScatterOp::create(
            builder, consumer.getLoc(), emitEdge.root, arrayIdAttr, ownerDims,
            blockShape, IntegerAttr::get(IntegerType::get(ctx, 64), 0),
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
      consumer.emitOpError()
          << "sde-redistribute: unexpected redistribution edge kind";
      sawFailure = true;
    }
    if (sawFailure)
      signalPassFailure();
  }
};
} // namespace
namespace mlir::carts::sde {
std::unique_ptr<Pass> createSdeRedistributePass() {
  return std::make_unique<SdeRedistributePass>();
}
} // namespace mlir::carts::sde
