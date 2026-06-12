#ifndef CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
#define CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/CuMuGraphPartitioning.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <limits>

namespace mlir::carts::sde {

/// True once an op carries committed CU/MU physical layout facts.
inline bool hasCommittedCuMuPartitionFacts(Operation *op) {
  auto iterate = dyn_cast_or_null<SdeSuIterateOp>(op);
  if (!iterate)
    return false;
  return iterate.getPhysicalOwnerDimsAttr() ||
         iterate.getPhysicalBlockShapeAttr() ||
         iterate.getLogicalWorkerSliceAttr() ||
         iterate.getPhysicalHaloShapeAttr() ||
         iterate.getIterationTopologyAttr() ||
         iterate.getDistributionKindAttr();
}

/// True when a stencil carries N-D owner/access facts that cannot be realized
/// by the current SDE loop rank. Physical tiling passes use this to fail closed
/// instead of replacing those facts with one-dimensional fallback grain.
inline bool requiresNestedStencilOwnerPromotion(SdeSuIterateOp op) {
  auto classification = op.getStructuredClassification();
  if (!classification ||
      *classification != SdeStructuredClassification::stencil)
    return false;

  unsigned loopRank = op.getLowerBounds().size();
  auto ownerDims = readI64ArrayAttr(op.getOwnerDimsAttr());
  auto minOffsets = readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  if (!ownerDims || !minOffsets || !maxOffsets)
    return false;

  if (ownerDims->size() > loopRank || minOffsets->size() > loopRank ||
      maxOffsets->size() > loopRank)
    return true;
  for (int64_t ownerDim : *ownerDims)
    if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= loopRank)
      return true;
  return false;
}

inline bool sameI64Values(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin());
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
        fact->layoutKind != ArrayLayoutKind::blockParallel) {
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
      if (name == ownerDimsName || name == blockShapeName ||
          name == muBlockCountName || name == budgetBlockShapeName)
        continue;
      fields.push_back(named);
    }

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

inline bool reconcileArrayLayoutWithCommittedPhysicalShape(SdeSuIterateOp op) {
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(op.getPhysicalOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(op.getPhysicalBlockShapeAttr());
  if (!ownerDims || ownerDims->empty() || !blockShape || blockShape->empty())
    return false;
  return rewriteWriterArrayLayoutToPhysicalShape(op, *ownerDims, *blockShape);
}

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_SDECOMMITTEDFACTUTILS_H
