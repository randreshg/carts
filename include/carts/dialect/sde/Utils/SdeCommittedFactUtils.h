#ifndef CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
#define CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <limits>

namespace mlir::carts::sde {

inline std::optional<LayoutGraphFact>
findSingleCommittedWriterBlockLayout(SdeSuIterateOp op);

/// Physical block layout recovered from arrayLayout facts or rank-expanded roots.
struct CommittedSuPhysicalLayout {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> blockShape;
};

inline std::optional<CommittedSuPhysicalLayout>
recoverPhysicalLayoutFromCuGroupCounts(SdeSuIterateOp op);


inline std::optional<CommittedSuPhysicalLayout>
recoverCommittedPhysicalLayout(SdeSuIterateOp op) {
  if (!op)
    return std::nullopt;
  if (!op.getBody().empty()) {
    for (SdeArrayLayoutRootOp root :
         op.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
      if (root.getMode() != SdeAccessMode::write)
        continue;
      auto muType = dyn_cast<MemRefType>(root.getRoot().getType());
      if (!muType || !isCommittedRankExpandedMuType(muType))
        continue;
      if (std::optional<RecoveredMuPhysicalLayout> recovered =
              recoverMuPhysicalLayoutFromExpandedType(muType)) {
        CommittedSuPhysicalLayout layout;
        layout.ownerDims.reserve(recovered->ownerDims.size());
        for (unsigned dim : recovered->ownerDims)
          layout.ownerDims.push_back(static_cast<int64_t>(dim));
        layout.blockShape.assign(recovered->physicalBlockShape.begin(),
                                 recovered->physicalBlockShape.end());
        return layout;
      }
    }
  }
  if (std::optional<CommittedSuPhysicalLayout> fromCu =
          recoverPhysicalLayoutFromCuGroupCounts(op))
    return fromCu;
  if (std::optional<LayoutGraphFact> writeLayout =
          findSingleCommittedWriterBlockLayout(op))
    return CommittedSuPhysicalLayout{writeLayout->ownerDims,
                                     writeLayout->blockShape};
  return std::nullopt;
}

inline std::optional<CommittedSuPhysicalLayout>
recoverPhysicalLayoutFromCuGroupCounts(SdeSuIterateOp op) {
  SdeCuRegionOp cu = findSuComputeCuRegion(op);
  if (!cu)
    return std::nullopt;
  auto groupCounts = readI64ArrayAttr(cu.getGroupBlockCountAttr());
  if (!groupCounts || groupCounts->empty())
    return std::nullopt;

  std::optional<LoopIndexedOutputShape> outputPlan =
      findConsistentLoopIndexedOutputShapeWithOwnerDims(op);
  if (!outputPlan)
    outputPlan = findLoopIndexedOutputShape(op);
  if (!outputPlan || outputPlan->shape.empty())
    return std::nullopt;

  SmallVector<int64_t, 4> ownerDims;
  if (!outputPlan->ownerPhysicalDims.empty())
    ownerDims.assign(outputPlan->ownerPhysicalDims.begin(),
                     outputPlan->ownerPhysicalDims.end());
  if (ownerDims.empty()) {
    if (std::optional<SuNeighborhoodAccessInfo> neighborhood =
            queryNeighborhoodAccessInfo(op))
      ownerDims.assign(neighborhood->ownerDims.begin(),
                       neighborhood->ownerDims.end());
  }
  if (ownerDims.size() != groupCounts->size())
    return std::nullopt;

  SmallVector<int64_t, 4> blockShape(outputPlan->shape.begin(),
                                     outputPlan->shape.end());
  for (auto [slot, ownerDim] : llvm::enumerate(ownerDims)) {
    if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= blockShape.size())
      return std::nullopt;
    int64_t gridCount = (*groupCounts)[slot];
    if (gridCount <= 0 || blockShape[ownerDim] % gridCount != 0)
      return std::nullopt;
    blockShape[ownerDim] = blockShape[ownerDim] / gridCount;
  }
  return CommittedSuPhysicalLayout{ownerDims, blockShape};
}

inline bool hasCommittedSuPhysicalLayout(SdeSuIterateOp op) {
  return recoverCommittedPhysicalLayout(op).has_value();
}

inline std::optional<SdeIterationTopology>
deriveIterationTopology(SdeSuIterateOp op) {
  std::optional<CommittedSuPhysicalLayout> layout =
      recoverCommittedPhysicalLayout(op);
  if (!layout || layout->ownerDims.empty())
    return std::nullopt;
  if (layout->ownerDims.size() == 1)
    return SdeIterationTopology::owner_strip;
  if (layout->ownerDims.size() >= 2)
    return SdeIterationTopology::owner_tile;
  return std::nullopt;
}

inline std::optional<SmallVector<int64_t, 4>>
deriveCommittedHaloShape(SdeSuIterateOp op) {
  std::optional<SuNeighborhoodAccessInfo> neighborhood =
      queryNeighborhoodAccessInfo(op);
  if (!neighborhood)
    return std::nullopt;
  SmallVector<int64_t, 4> halo;
  bool nonZero = false;
  for (auto [minOffset, maxOffset] :
       llvm::zip_equal(neighborhood->minOffsets, neighborhood->maxOffsets)) {
    int64_t width =
        std::max<int64_t>(std::llabs(minOffset), std::llabs(maxOffset));
    nonZero |= width != 0;
    halo.push_back(width);
  }
  return nonZero ? std::optional<SmallVector<int64_t, 4>>(std::move(halo))
                 : std::nullopt;
}

/// True when a writer SU has committed block layout in transitional arrayLayout
/// facts with explicit owner dims (replicated/contraction-without-owner stay
/// uncommitted until DistributionPlanning realizes loop-step grain).
inline bool hasCommittedWriterBlockLayout(SdeSuIterateOp op) {
  if (!op)
    return false;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
    if (fact.role == LayoutGraphRole::write && !fact.ownerDims.empty() &&
        !fact.blockShape.empty())
      return true;
  }
  return false;
}

inline std::optional<LayoutGraphFact>
findSingleCommittedWriterBlockLayout(SdeSuIterateOp op) {
  if (!op)
    return std::nullopt;
  std::optional<LayoutGraphFact> selected;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
    if (fact.role != LayoutGraphRole::write || fact.ownerDims.empty() ||
        fact.blockShape.empty())
      continue;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->layoutKind != fact.layoutKind ||
        selected->ownerDims != fact.ownerDims ||
        selected->blockShape != fact.blockShape)
      return std::nullopt;
  }
  return selected;
}

/// True once an op carries committed CU/MU physical layout facts.
inline bool hasCommittedCuMuPartitionFacts(Operation *op) {
  auto iterate = dyn_cast_or_null<SdeSuIterateOp>(op);
  if (!iterate)
    return false;
  if (SdeCuRegionOp cu = findSuComputeCuRegion(iterate))
    if (cu.getGroupBlockCountAttr())
      return true;
  return hasCommittedSuPhysicalLayout(iterate);
}

/// Commit CU grouping as per-owner-dim block counts on the compute `cu_region`.
/// Omits the attribute when every owner dim groups exactly one DB block.
inline void commitCuGroupBlockCounts(SdeCuRegionOp cu,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> physicalBlockShape,
                                     ArrayRef<int64_t> logicalWorkerSlice = {}) {
  if (!cu || ownerDims.empty() || physicalBlockShape.empty())
    return;
  ArrayRef<int64_t> slice =
      logicalWorkerSlice.empty() ? physicalBlockShape : logicalWorkerSlice;
  SmallVector<int64_t, 4> counts;
  counts.reserve(ownerDims.size());
  bool anyGrouped = false;
  for (int64_t rawDim : ownerDims) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= slice.size() ||
        static_cast<size_t>(rawDim) >= physicalBlockShape.size())
      return;
    int64_t blockSize = physicalBlockShape[rawDim];
    int64_t span = slice[rawDim];
    if (span <= 0 || blockSize <= 0 || span % blockSize != 0)
      return;
    int64_t count = span / blockSize;
    counts.push_back(count);
    anyGrouped |= count > 1;
  }
  if (!anyGrouped) {
    cu.removeGroupBlockCountAttr();
    return;
  }
  cu.setGroupBlockCountAttr(buildI64ArrayAttr(cu.getContext(), counts));
}

inline void commitCuGroupBlockCounts(SdeSuIterateOp op,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> physicalBlockShape,
                                     ArrayRef<int64_t> logicalWorkerSlice = {}) {
  commitCuGroupBlockCounts(findSuComputeCuRegion(op), ownerDims,
                           physicalBlockShape, logicalWorkerSlice);
}

/// True when a stencil carries N-D owner/access facts that cannot be realized
/// by the current SDE loop rank. Physical tiling passes use this to fail closed
/// instead of replacing those facts with one-dimensional fallback grain.
inline bool requiresNestedStencilOwnerPromotion(SdeSuIterateOp op) {
  auto classification = queryStructuredClassification(op);
  if (!classification ||
      *classification != SdeStructuredClassification::stencil)
    return false;

  unsigned loopRank = op.getLowerBounds().size();
  auto neighborhood = queryNeighborhoodAccessInfo(op);
  if (!neighborhood)
    return false;

  if (neighborhood->ownerDims.size() > loopRank ||
      neighborhood->minOffsets.size() > loopRank ||
      neighborhood->maxOffsets.size() > loopRank)
    return true;
  for (int64_t ownerDim : neighborhood->ownerDims)
    if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= loopRank)
      return true;
  return false;
}

inline bool sameI64Values(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

inline bool containsI64Value(ArrayRef<int64_t> values, int64_t needle) {
  return llvm::is_contained(values, needle);
}

inline std::optional<SmallVector<int64_t, 4>>
findWriteArrayRootShape(SdeSuIterateOp op, int64_t arrayId) {
  if (!op || op.getBody().empty())
    return std::nullopt;
  for (SdeArrayLayoutRootOp root :
       op.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
    if (static_cast<int64_t>(root.getArrayId()) != arrayId ||
        root.getMode() != SdeAccessMode::write)
      continue;
    auto type = dyn_cast<MemRefType>(root.getRoot().getType());
    if (!type || !type.hasStaticShape())
      return std::nullopt;
    return SmallVector<int64_t, 4>(type.getShape().begin(),
                                   type.getShape().end());
  }
  return std::nullopt;
}

/// Resolve module-stable array id from SSA-reachable layout-root facts.
inline std::optional<int64_t> getMuArrayIdFromLayoutRoot(SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (auto root = dyn_cast<SdeArrayLayoutRootOp>(user))
      return static_cast<int64_t>(root.getArrayId());
  }
  return std::nullopt;
}

inline Value findArrayLayoutRoot(SdeSuIterateOp op, int64_t arrayId,
                                 SdeAccessMode mode) {
  if (!op || op.getBody().empty())
    return Value();
  for (SdeArrayLayoutRootOp root :
       op.getBody().front().getOps<SdeArrayLayoutRootOp>()) {
    if (static_cast<int64_t>(root.getArrayId()) == arrayId &&
        root.getMode() == mode)
      return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(root.getRoot());
  }
  return Value();
}

inline std::optional<SmallVector<int64_t, 4>>
collapseRankExpandedRootShape(ArrayRef<int64_t> rootShape,
                              ArrayRef<int64_t> ownerDims,
                              ArrayRef<int64_t> physicalBlockShape) {
  if (rootShape.size() == physicalBlockShape.size())
    return SmallVector<int64_t, 4>(rootShape.begin(), rootShape.end());
  if (rootShape.size() != ownerDims.size() + physicalBlockShape.size())
    return std::nullopt;

  SmallVector<int64_t, 4> logicalShape(physicalBlockShape.begin(),
                                       physicalBlockShape.end());
  for (auto [slot, rawDim] : llvm::enumerate(ownerDims)) {
    if (rawDim < 0 || static_cast<size_t>(rawDim) >= logicalShape.size())
      return std::nullopt;
    int64_t gridExtent = rootShape[slot];
    int64_t blockExtent = physicalBlockShape[rawDim];
    if (gridExtent <= 0 || blockExtent <= 0 ||
        gridExtent > std::numeric_limits<int64_t>::max() / blockExtent)
      return std::nullopt;
    logicalShape[rawDim] = gridExtent * blockExtent;
  }
  return logicalShape;
}

/// Make write-role arrayLayout facts reflect the committed physical MU grain.
inline bool
rewriteWriterArrayLayoutToPhysicalShape(SdeSuIterateOp op,
                                        ArrayRef<int64_t> ownerDims,
                                        ArrayRef<int64_t> physicalBlockShape) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty())
    return false;
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return false;

  MLIRContext *ctx = op.getContext();
  Builder builder(ctx);
  StringAttr blockShapeName =
      builder.getStringAttr(AttrNames::LayoutGraph::BlockShape);
  StringAttr ownerDimsName =
      builder.getStringAttr(AttrNames::LayoutGraph::OwnerDims);
  StringAttr kindName = builder.getStringAttr(AttrNames::LayoutGraph::Kind);
  StringAttr muBlockCountName =
      builder.getStringAttr(AttrNames::LayoutGraph::MuBlockCount);
  StringAttr budgetBlockShapeName =
      builder.getStringAttr(AttrNames::LayoutGraph::BudgetBlockShape);

  bool changed = false;
  SmallVector<Attribute, 4> rewritten;
  rewritten.reserve(layout.size());
  for (Attribute attr : layout) {
    auto dict = dyn_cast<DictionaryAttr>(attr);
    std::optional<LayoutGraphFact> fact =
        dict ? parseArrayLayoutFact(dict) : std::nullopt;
    if (!dict || !fact || fact->role != LayoutGraphRole::write ||
        (fact->layoutKind != ArrayLayoutKind::blockParallel &&
         fact->layoutKind != ArrayLayoutKind::blockContraction &&
         fact->layoutKind != ArrayLayoutKind::replicated)) {
      rewritten.push_back(attr);
      continue;
    }

    std::optional<SmallVector<int64_t, 4>> rootShape =
        findWriteArrayRootShape(op, fact->id);
    if (!rootShape) {
      rewritten.push_back(attr);
      continue;
    }
    std::optional<SmallVector<int64_t, 4>> logicalRootShape =
        collapseRankExpandedRootShape(*rootShape, ownerDims,
                                      physicalBlockShape);
    if (!logicalRootShape) {
      rewritten.push_back(attr);
      continue;
    }

    SmallVector<NamedAttribute, 8> fields;
    fields.reserve(dict.size());
    for (NamedAttribute named : dict) {
      StringAttr name = named.getName();
      if (name == kindName || name == ownerDimsName || name == blockShapeName ||
          name == muBlockCountName || name == budgetBlockShapeName)
        continue;
      fields.push_back(named);
    }

    fields.push_back(builder.getNamedAttr(
        kindName,
        builder.getStringAttr(AttrNames::LayoutGraph::BlockParallel)));
    fields.push_back(
        builder.getNamedAttr(ownerDimsName, buildI64ArrayAttr(ctx, ownerDims)));
    fields.push_back(builder.getNamedAttr(
        blockShapeName, buildI64ArrayAttr(ctx, physicalBlockShape)));
    int64_t blockCount = inferCuCountFromMuPartition(
        *logicalRootShape, ownerDims, physicalBlockShape);
    if (blockCount > 0)
      fields.push_back(builder.getNamedAttr(
          muBlockCountName, builder.getI64IntegerAttr(blockCount)));
    rewritten.push_back(builder.getDictionaryAttr(fields));
    changed = true;
  }

  if (changed)
    op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
  return changed;
}

inline bool reconcilePartialReductionOwnersWithCommittedShape(
    SdeSuIterateOp op, ArrayRef<int64_t> committedOwnerDims) {
  if (!op || committedOwnerDims.empty())
    return false;
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != SdeStructuredClassification::elementwise_pipeline ||
      !op.getPartialReductionAttr())
    return false;

  std::optional<SmallVector<int64_t, 4>> reductionOwnerDims =
      readI64ArrayAttr(op.getPartialReductionOwnerDimsAttr());
  if (!reductionOwnerDims || reductionOwnerDims->empty())
    return false;

  SmallVector<int64_t, 4> reconciled;
  for (int64_t dim : *reductionOwnerDims) {
    if (!containsI64Value(committedOwnerDims, dim))
      continue;
    if (!containsI64Value(reconciled, dim))
      reconciled.push_back(dim);
  }
  if (reconciled.empty() || sameI64Values(reconciled, *reductionOwnerDims))
    return false;

  op.setPartialReductionOwnerDimsAttr(
      buildI64ArrayAttr(op.getContext(), reconciled));
  return true;
}

/// Commit physical grain via CU group counts only; rank expansion stays in
/// RankExpandMu / MemoryUnitRealization.
inline bool commitWriterPhysicalLayoutViaMuType(
    SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape,
    ArrayRef<int64_t> logicalWorkerSlice = {}) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty())
    return false;
  commitCuGroupBlockCounts(op, ownerDims, physicalBlockShape,
                           logicalWorkerSlice);
  bool changed =
      reconcilePartialReductionOwnersWithCommittedShape(op, ownerDims);
  changed |= rewriteWriterArrayLayoutToPhysicalShape(op, ownerDims,
                                                       physicalBlockShape);
  return changed;
}

inline bool commitWriterPhysicalLayoutFacts(
    SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape,
    ArrayRef<int64_t> logicalWorkerSlice = {}) {
  return commitWriterPhysicalLayoutViaMuType(op, ownerDims, physicalBlockShape,
                                             logicalWorkerSlice);
}

inline bool reconcileArrayLayoutWithCommittedPhysicalShape(SdeSuIterateOp op) {
  std::optional<LayoutGraphFact> writeLayout =
      findSingleCommittedWriterBlockLayout(op);
  if (!writeLayout)
    return false;
  return rewriteWriterArrayLayoutToPhysicalShape(
      op, writeLayout->ownerDims, writeLayout->blockShape);
}

inline bool factMatchesCommittedWriterShape(const LayoutGraphFact &fact,
                                            const LayoutGraphFact &home) {
  return fact.layoutKind == home.layoutKind &&
         sameI64Values(fact.ownerDims, home.ownerDims) &&
         sameI64Values(fact.blockShape, home.blockShape) &&
         fact.muBlockCount == home.muBlockCount;
}

inline bool
reconcileReaderArrayLayoutsWithCommittedWriterShapes(Operation *moduleOp) {
  if (!moduleOp)
    return false;

  struct WriterHome {
    LayoutGraphFact fact;
    Value root;
  };

  llvm::DenseMap<int64_t, WriterHome> homes;
  llvm::DenseSet<int64_t> conflictingHomes;
  moduleOp->walk([&](SdeSuIterateOp op) {
    for (const LayoutGraphFact &fact :
         parseArrayLayoutFacts(op.getArrayLayoutAttr())) {
      if (fact.role != LayoutGraphRole::write ||
          fact.layoutKind != ArrayLayoutKind::blockParallel ||
          fact.ownerDims.empty() || fact.blockShape.empty())
        continue;
      Value root = findArrayLayoutRoot(op, fact.id, SdeAccessMode::write);
      if (!root)
        continue;
      auto inserted = homes.try_emplace(fact.id, WriterHome{fact, root});
      if (!inserted.second && (!factMatchesCommittedWriterShape(
                                   fact, inserted.first->second.fact) ||
                               !::mlir::carts::ValueAnalysis::sameMemrefRoot(
                                   root, inserted.first->second.root)))
        conflictingHomes.insert(fact.id);
    }
  });

  bool changed = false;
  moduleOp->walk([&](SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    Builder builder(op.getContext());
    StringAttr kindName = builder.getStringAttr(AttrNames::LayoutGraph::Kind);
    StringAttr ownerDimsName =
        builder.getStringAttr(AttrNames::LayoutGraph::OwnerDims);
    StringAttr blockShapeName =
        builder.getStringAttr(AttrNames::LayoutGraph::BlockShape);
    StringAttr muBlockCountName =
        builder.getStringAttr(AttrNames::LayoutGraph::MuBlockCount);
    StringAttr budgetBlockShapeName =
        builder.getStringAttr(AttrNames::LayoutGraph::BudgetBlockShape);

    SmallVector<Attribute, 4> rewritten;
    rewritten.reserve(layout.size());
    bool opChanged = false;
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<LayoutGraphFact> fact =
          dict ? parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !fact || fact->role != LayoutGraphRole::read) {
        rewritten.push_back(attr);
        continue;
      }
      auto homeIt = homes.find(fact->id);
      if (homeIt == homes.end() || conflictingHomes.contains(fact->id) ||
          fact->commVolumeBytes != 0 ||
          factMatchesCommittedWriterShape(*fact, homeIt->second.fact)) {
        rewritten.push_back(attr);
        continue;
      }
      Value readRoot = findArrayLayoutRoot(op, fact->id, SdeAccessMode::read);
      if (!readRoot || !::mlir::carts::ValueAnalysis::sameMemrefRoot(
                           readRoot, homeIt->second.root)) {
        rewritten.push_back(attr);
        continue;
      }

      SmallVector<NamedAttribute, 8> fields;
      fields.reserve(dict.size());
      for (NamedAttribute named : dict) {
        StringAttr name = named.getName();
        if (name == kindName || name == ownerDimsName ||
            name == blockShapeName || name == muBlockCountName ||
            name == budgetBlockShapeName)
          continue;
        fields.push_back(named);
      }
      fields.push_back(builder.getNamedAttr(
          kindName, builder.getStringAttr(
                        stringifyLayoutKind(homeIt->second.fact.layoutKind))));
      fields.push_back(builder.getNamedAttr(
          ownerDimsName,
          buildI64ArrayAttr(op.getContext(), homeIt->second.fact.ownerDims)));
      fields.push_back(builder.getNamedAttr(
          blockShapeName,
          buildI64ArrayAttr(op.getContext(), homeIt->second.fact.blockShape)));
      fields.push_back(builder.getNamedAttr(
          muBlockCountName,
          builder.getI64IntegerAttr(homeIt->second.fact.muBlockCount)));
      rewritten.push_back(builder.getDictionaryAttr(fields));
      opChanged = true;
    }

    if (opChanged) {
      op.setArrayLayoutAttr(ArrayAttr::get(op.getContext(), rewritten));
      changed = true;
    }
  });
  return changed;
}

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
