///==========================================================================///
/// File: MuLayoutRewriter.cpp
///
/// Implementation of the SDE MU-level rank-expansion rewriter (see
/// MuLayoutRewriter.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {

std::unique_ptr<MuAccessIndexer>
makeMuAccessIndexer(SdeStructuredClassification cls,
                    const MuPhysicalLayout &layout) {
  switch (cls) {
  case SdeStructuredClassification::stencil:
    return std::make_unique<MuStencilIndexer>(layout);
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
    return std::make_unique<MuElementWiseIndexer>(layout);
  case SdeStructuredClassification::matmul:
  case SdeStructuredClassification::reduction:
    return std::make_unique<MuBlockIndexer>(layout);
  }
  return std::make_unique<MuBlockIndexer>(layout);
}

bool supportsRankExpandedAccessWindows(SdeSuIterateOp si) {
  if (!si)
    return false;
  std::optional<SdeStructuredClassification> cls =
      si.getStructuredClassification();
  if (!cls)
    return false;
  switch (*cls) {
  case SdeStructuredClassification::elementwise:
  case SdeStructuredClassification::elementwise_pipeline:
  case SdeStructuredClassification::matmul:
  case SdeStructuredClassification::stencil:
    return true;
  case SdeStructuredClassification::reduction:
    return si.getReductionAccumulators().empty();
  }
  return false;
}

bool isBlockGridRealizable(SdeSuIterateOp si, MemRefType logicalType,
                           MuPhysicalLayout &out) {
  if (!supportsRankExpandedAccessWindows(si))
    return false;

  std::optional<MuPhysicalLayout> layout =
      resolveMuPhysicalLayout(logicalType, si.getPhysicalOwnerDimsAttr(),
                              si.getPhysicalBlockShapeAttr());
  if (!layout)
    return false;
  out = *layout;
  return true;
}

struct ComparableBlockGrid {
  unsigned logicalRank = 0;
  llvm::SmallVector<unsigned, 4> ownerDims;
  llvm::SmallVector<int64_t, 4> blockExtents;
  llvm::SmallVector<int64_t, 4> gridCounts;
};

static ComparableBlockGrid comparableBlockGrid(const MuPhysicalLayout &layout) {
  ComparableBlockGrid result;
  result.logicalRank = layout.logicalRank();
  result.ownerDims.assign(layout.ownerDims.begin(), layout.ownerDims.end());
  result.blockExtents.assign(layout.blockExtents.begin(),
                             layout.blockExtents.end());
  result.gridCounts.assign(layout.blockCounts.begin(),
                           layout.blockCounts.end());
  return result;
}

static ComparableBlockGrid
comparableBlockGrid(const ExpandedBlockGridMu &layout) {
  ComparableBlockGrid result;
  result.logicalRank = layout.logicalRank;
  result.ownerDims.assign(layout.ownerDims.begin(), layout.ownerDims.end());
  result.blockExtents.assign(layout.blockExtents.begin(),
                             layout.blockExtents.end());
  result.gridCounts.assign(layout.gridCounts.begin(), layout.gridCounts.end());
  return result;
}

static std::optional<int64_t> getMuArrayId(SdeMuAllocOp muAlloc) {
  if (IntegerAttr arrayId = muAlloc.getArrayIdAttr())
    return arrayId.getInt();
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (auto root = dyn_cast<SdeArrayLayoutRootOp>(user))
      return static_cast<int64_t>(root.getArrayId());
  }
  return std::nullopt;
}

static std::optional<LayoutGraphFact> findWriteLayoutFact(SdeSuIterateOp si,
                                                          int64_t arrayId) {
  if (!si)
    return std::nullopt;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(si.getArrayLayoutAttr())) {
    if (fact.id == arrayId && fact.role == LayoutGraphRole::write &&
        fact.layoutKind == ArrayLayoutKind::blockParallel &&
        !fact.ownerDims.empty())
      return fact;
  }
  return std::nullopt;
}

static std::optional<MuPhysicalLayout>
resolveMuPhysicalLayoutFromFact(MemRefType logicalType,
                                const LayoutGraphFact &fact) {
  if (!fact.budgetBlockShape.empty())
    if (std::optional<MuPhysicalLayout> layout = resolveMuPhysicalLayout(
            logicalType, fact.ownerDims, fact.budgetBlockShape))
      return layout;
  return resolveMuPhysicalLayout(logicalType, fact.ownerDims, fact.blockShape);
}

static std::optional<MuPhysicalLayout>
resolveMuPhysicalLayoutForWriter(MemRefType logicalType, SdeSuIterateOp writer,
                                 const LayoutGraphFact &fact) {
  if (writer)
    if (std::optional<MuPhysicalLayout> layout = resolveMuPhysicalLayout(
            logicalType, writer.getPhysicalOwnerDimsAttr(),
            writer.getPhysicalBlockShapeAttr()))
      return layout;
  return resolveMuPhysicalLayoutFromFact(logicalType, fact);
}

static std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMuFromShape(ArrayRef<int64_t> ownerVals,
                                      ArrayRef<int64_t> blockVals,
                                      MemRefType muType) {
  if (!muType || !muType.hasStaticShape() || ownerVals.empty() ||
      blockVals.empty())
    return std::nullopt;

  const unsigned numOwner = ownerVals.size();
  const unsigned logicalRank = blockVals.size();
  if (logicalRank + numOwner != static_cast<unsigned>(muType.getRank()))
    return std::nullopt;

  SmallVector<bool, 4> seen(logicalRank, false);
  SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(numOwner);
  for (int64_t rawDim : ownerVals) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) >= logicalRank)
      return std::nullopt;
    if (seen[rawDim])
      return std::nullopt;
    seen[rawDim] = true;
    ownerDims.push_back(static_cast<unsigned>(rawDim));
  }
  llvm::sort(ownerDims);

  ArrayRef<int64_t> shape = muType.getShape();
  ArrayRef<int64_t> tiles = shape.drop_front(numOwner);
  if (tiles.size() != logicalRank)
    return std::nullopt;
  for (unsigned dim = 0; dim < logicalRank; ++dim)
    if (tiles[dim] != blockVals[dim])
      return std::nullopt;

  ExpandedBlockGridMu out;
  out.ownerDims = std::move(ownerDims);
  out.logicalRank = logicalRank;
  out.blockExtents.reserve(numOwner);
  out.gridCounts.reserve(numOwner);
  for (unsigned i = 0; i < numOwner; ++i) {
    int64_t blockExtent = blockVals[out.ownerDims[i]];
    if (blockExtent <= 0)
      return std::nullopt;
    out.blockExtents.push_back(blockExtent);
    out.gridCounts.push_back(shape[i]);
  }
  return out;
}

static std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMuFromFact(const LayoutGraphFact &fact,
                                     MemRefType muType) {
  if (!fact.budgetBlockShape.empty())
    if (std::optional<ExpandedBlockGridMu> expanded =
            recognizeExpandedBlockGridMuFromShape(
                fact.ownerDims, fact.budgetBlockShape, muType))
      return expanded;
  return recognizeExpandedBlockGridMuFromShape(fact.ownerDims, fact.blockShape,
                                               muType);
}

static std::optional<ExpandedBlockGridMu> recognizeExpandedBlockGridMuForWriter(
    SdeSuIterateOp writer, const LayoutGraphFact &fact, MemRefType muType) {
  if (writer) {
    std::optional<SmallVector<int64_t, 4>> ownerVals =
        readI64ArrayAttr(writer.getPhysicalOwnerDimsAttr());
    std::optional<SmallVector<int64_t, 4>> blockVals =
        readI64ArrayAttr(writer.getPhysicalBlockShapeAttr());
    if (ownerVals && blockVals)
      if (std::optional<ExpandedBlockGridMu> expanded =
              recognizeExpandedBlockGridMuFromShape(*ownerVals, *blockVals,
                                                    muType))
        return expanded;
  }
  return recognizeExpandedBlockGridMuFromFact(fact, muType);
}

static std::optional<ComparableBlockGrid> resolveComparableBlockGridForWriter(
    SdeSuIterateOp writer, const LayoutGraphFact &fact, MemRefType muType) {
  if (!muType)
    return std::nullopt;
  if (std::optional<MuPhysicalLayout> flat =
          resolveMuPhysicalLayoutForWriter(muType, writer, fact))
    return comparableBlockGrid(*flat);
  if (std::optional<ExpandedBlockGridMu> expanded =
          recognizeExpandedBlockGridMuForWriter(writer, fact, muType))
    return comparableBlockGrid(*expanded);
  return std::nullopt;
}

static bool samePhysicalLayout(const ComparableBlockGrid &lhs,
                               const ComparableBlockGrid &rhs) {
  return lhs.logicalRank == rhs.logicalRank && lhs.ownerDims == rhs.ownerDims &&
         lhs.blockExtents == rhs.blockExtents &&
         lhs.gridCounts == rhs.gridCounts;
}

llvm::SmallVector<Value, 6> MuBlockIndexer::localize(ValueRange logicalIndices,
                                                     OpBuilder &builder,
                                                     Location loc) const {
  // Owner dim -> committed block extent (constant index value, built lazily).
  llvm::SmallVector<std::optional<int64_t>, 4> ownerBlockForDim(
      layout.logicalRank());
  for (auto [slot, dim] : llvm::enumerate(layout.ownerDims))
    ownerBlockForDim[dim] = layout.blockExtents[slot];

  auto blockConst = [&](int64_t extent) -> Value {
    return arith::ConstantIndexOp::create(builder, loc, extent);
  };

  llvm::SmallVector<Value, 6> result;
  result.reserve(layout.expandedRank());

  // Prefix grid coordinates: gridCoord = idx / B, in committed owner order.
  for (auto [slot, dim] : llvm::enumerate(layout.ownerDims)) {
    Value g = logicalIndices[dim];
    Value b = blockConst(layout.blockExtents[slot]);
    result.push_back(arith::DivUIOp::create(builder, loc, g, b));
  }

  // Tile coordinates: owner dim -> idx % B, non-owner dim -> passthrough.
  for (unsigned d = 0; d < layout.logicalRank(); ++d) {
    if (ownerBlockForDim[d]) {
      Value g = logicalIndices[d];
      Value b = blockConst(*ownerBlockForDim[d]);
      result.push_back(arith::RemUIOp::create(builder, loc, g, b));
    } else {
      result.push_back(logicalIndices[d]);
    }
  }

  return result;
}

// A writer whose entire iteration domain folds to constants is the strongest
// witness for the non-tautological grid-count proof: its bounds literally name
// the committed logical extent. Writers whose bounds came in as a dynamic
// `index_cast` of a source parameter (the same logical extent the array was
// allocated with, but un-propagated by the frontend) cannot serve that proof.
static bool hasStaticIterationDomain(SdeSuIterateOp si) {
  for (auto [lb, ub] : llvm::zip(si.getLowerBounds(), si.getUpperBounds())) {
    if (!ValueAnalysis::tryFoldConstantIndex(lb) ||
        !ValueAnalysis::tryFoldConstantIndex(ub))
      return false;
  }
  return true;
}

SdeSuIterateOp findCommittedBlockLayoutWriter(SdeMuAllocOp muAlloc) {
  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType)
    return SdeSuIterateOp();
  std::optional<int64_t> arrayId = getMuArrayId(muAlloc);
  if (!arrayId)
    return SdeSuIterateOp();

  SdeSuIterateOp result;
  SdeSuIterateOp staticResult;
  SdeSuIterateOp supportedResult;
  SdeSuIterateOp staticSupportedResult;
  std::optional<ComparableBlockGrid> committedLayout;
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    SdeSuIterateOp si;
    std::optional<LayoutGraphFact> fact;
    for (SdeSuIterateOp current = user->getParentOfType<SdeSuIterateOp>();
         current; current = current->getParentOfType<SdeSuIterateOp>()) {
      fact = findWriteLayoutFact(current, *arrayId);
      if (fact) {
        si = current;
        break;
      }
    }
    if (!si || !fact)
      continue;
    std::optional<ComparableBlockGrid> layout =
        resolveComparableBlockGridForWriter(si, *fact, muType);
    if (!layout)
      continue;
    if (!result) {
      result = si;
      committedLayout = *layout;
    } else if (!samePhysicalLayout(*committedLayout, *layout)) {
      return SdeSuIterateOp();
    }
    // Among writers that committed the identical block layout, retain the first
    // with a fully-static iteration domain so the shared grid-count proof has a
    // constant witness even when a later writer's bound is a dynamic source
    // parameter. The committed owner-dims/block-shape are the same either way.
    if (!staticResult && hasStaticIterationDomain(si))
      staticResult = si;
    if (!supportsRankExpandedAccessWindows(si))
      continue;
    if (!supportedResult)
      supportedResult = si;
    if (!staticSupportedResult && hasStaticIterationDomain(si))
      staticSupportedResult = si;
  }
  if (staticSupportedResult)
    return staticSupportedResult;
  if (supportedResult)
    return supportedResult;
  return staticResult ? staticResult : result;
}

std::optional<CommittedMuBlockLayout>
findCommittedMuBlockLayout(SdeMuAllocOp muAlloc) {
  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType)
    return std::nullopt;
  std::optional<int64_t> arrayId = getMuArrayId(muAlloc);
  if (!arrayId)
    return std::nullopt;

  std::optional<CommittedMuBlockLayout> result;
  std::optional<CommittedMuBlockLayout> staticResult;
  std::optional<CommittedMuBlockLayout> supportedResult;
  std::optional<CommittedMuBlockLayout> staticSupportedResult;
  std::optional<ComparableBlockGrid> committedLayout;

  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    SdeSuIterateOp writer;
    std::optional<LayoutGraphFact> fact;
    for (SdeSuIterateOp current = user->getParentOfType<SdeSuIterateOp>();
         current; current = current->getParentOfType<SdeSuIterateOp>()) {
      fact = findWriteLayoutFact(current, *arrayId);
      if (fact) {
        writer = current;
        break;
      }
    }
    if (!writer || !fact)
      continue;

    std::optional<MuPhysicalLayout> layout =
        resolveMuPhysicalLayoutForWriter(muType, writer, *fact);
    if (!layout)
      continue;
    ComparableBlockGrid comparable = comparableBlockGrid(*layout);
    if (!result) {
      result = CommittedMuBlockLayout{writer, *layout};
      committedLayout = comparable;
    } else if (!samePhysicalLayout(*committedLayout, comparable)) {
      return std::nullopt;
    }

    CommittedMuBlockLayout current{writer, *layout};
    if (!staticResult && hasStaticIterationDomain(writer))
      staticResult = current;
    if (!supportsRankExpandedAccessWindows(writer))
      continue;
    if (!supportedResult)
      supportedResult = current;
    if (!staticSupportedResult && hasStaticIterationDomain(writer))
      staticSupportedResult = current;
  }

  if (staticSupportedResult)
    return staticSupportedResult;
  if (supportedResult)
    return supportedResult;
  if (staticResult)
    return staticResult;
  return result;
}

bool isBlockGridRealizable(SdeMuAllocOp muAlloc, MuPhysicalLayout &out) {
  std::optional<CommittedMuBlockLayout> committed =
      findCommittedMuBlockLayout(muAlloc);
  if (!committed || !supportsRankExpandedAccessWindows(committed->writer))
    return false;
  out = committed->layout;
  return true;
}

// Rank expansion preserves the allocation base pointer. Pointer comparisons can
// be repointed; dereferencing pointer uses keep the MU conservative.
static bool isLayoutInvariantBasePointer(polygeist::Memref2PointerOp m2p) {
  for (Operation *user : m2p.getResult().getUsers())
    if (!isa<LLVM::ICmpOp>(user))
      return false;
  return true;
}

bool muRootHasUnsupportedUse(Value root) {
  for (Operation *user : root.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, memref::DeallocOp,
            SdeArrayLayoutRootOp, SdeMuAccessWindowOp, SdeRedistOp>(user))
      continue;
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user))
      if (isLayoutInvariantBasePointer(m2p))
        continue;
    return true;
  }
  return false;
}

std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMu(SdeSuIterateOp si, MemRefType muType) {
  if (!si || !muType)
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> ownerVals =
      readI64ArrayAttr(si.getPhysicalOwnerDimsAttr());
  std::optional<llvm::SmallVector<int64_t, 4>> blockVals =
      readI64ArrayAttr(si.getPhysicalBlockShapeAttr());
  if (!ownerVals || blockVals == std::nullopt || ownerVals->empty())
    return std::nullopt;

  // Expanded form: K leading grid dims + L tile dims, with the rank-length
  // block-shape carrying the logical rank L.
  const unsigned numOwner = ownerVals->size();
  const unsigned logicalRank = blockVals->size();
  const unsigned muRank = muType.getRank();
  if (logicalRank + numOwner != muRank)
    return std::nullopt; // flat / owner-length / rank mismatch -> out of scope

  llvm::SmallVector<bool, 4> seen(logicalRank, false);
  llvm::SmallVector<unsigned, 4> ownerDims;
  ownerDims.reserve(numOwner);
  for (int64_t od : *ownerVals) {
    if (od < 0 || static_cast<unsigned>(od) >= logicalRank)
      return std::nullopt;
    if (seen[od])
      return std::nullopt;
    seen[od] = true;
    ownerDims.push_back(static_cast<unsigned>(od));
  }
  llvm::sort(ownerDims);

  ExpandedBlockGridMu out;
  out.ownerDims = std::move(ownerDims);
  out.logicalRank = logicalRank;
  out.blockExtents.reserve(numOwner);
  out.gridCounts.reserve(numOwner);
  ArrayRef<int64_t> shape = muType.getShape();
  for (unsigned i = 0; i < numOwner; ++i) {
    out.blockExtents.push_back((*blockVals)[out.ownerDims[i]]);
    out.gridCounts.push_back(shape[i]); // leading K grid dims, owner order
  }
  return out;
}

std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMu(SdeMuAllocOp muAlloc) {
  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType)
    return std::nullopt;
  std::optional<int64_t> arrayId = getMuArrayId(muAlloc);
  if (!arrayId)
    return std::nullopt;

  std::optional<ExpandedBlockGridMu> result;
  std::optional<ComparableBlockGrid> committedLayout;
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    if (!isa<memref::StoreOp>(user))
      continue;
    for (SdeSuIterateOp writer = user->getParentOfType<SdeSuIterateOp>();
         writer; writer = writer->getParentOfType<SdeSuIterateOp>()) {
      std::optional<LayoutGraphFact> fact =
          findWriteLayoutFact(writer, *arrayId);
      if (!fact)
        continue;
      std::optional<ExpandedBlockGridMu> expanded =
          recognizeExpandedBlockGridMuForWriter(writer, *fact, muType);
      if (!expanded)
        break;
      ComparableBlockGrid comparable = comparableBlockGrid(*expanded);
      if (!result) {
        result = *expanded;
        committedLayout = comparable;
      } else if (!samePhysicalLayout(*committedLayout, comparable)) {
        return std::nullopt;
      }
      break;
    }
  }
  return result;
}

std::optional<llvm::SmallVector<int64_t, 4>>
findOwnerIterationExtents(SdeSuIterateOp si,
                          llvm::ArrayRef<int64_t> blockExtents,
                          llvm::ArrayRef<int64_t> gridCounts) {
  if (!si || blockExtents.size() != gridCounts.size() || blockExtents.empty())
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> halo =
      readI64ArrayAttr(si.getPhysicalHaloShapeAttr());

  // Fold the committed iteration extents once (independent of the expanded
  // type) so each per-owner-dim proof is non-tautological.
  llvm::SmallVector<std::optional<int64_t>, 6> rawExtents;
  for (auto it :
       llvm::enumerate(llvm::zip(si.getLowerBounds(), si.getUpperBounds()))) {
    auto [lb, ub] = it.value();
    std::optional<int64_t> lbc = ValueAnalysis::tryFoldConstantIndex(lb);
    std::optional<int64_t> ubc = ValueAnalysis::tryFoldConstantIndex(ub);
    if (lbc && ubc)
      rawExtents.push_back(*ubc - *lbc);
    else
      rawExtents.push_back(std::nullopt);
  }

  // Match each owner dim's (block, grid) to a DISTINCT committed loop dim whose
  // extent (optionally halo-widened) ceilDivs to the grid count.
  const unsigned numOwner = blockExtents.size();
  llvm::SmallVector<int64_t, 4> result(numOwner, 0);
  llvm::SmallVector<bool, 6> used(rawExtents.size(), false);
  for (unsigned i = 0; i < numOwner; ++i) {
    int64_t blockExtent = blockExtents[i];
    int64_t gridExtent = gridCounts[i];
    if (blockExtent <= 0)
      return std::nullopt;
    bool matched = false;
    for (unsigned dim = 0; dim < rawExtents.size(); ++dim) {
      if (used[dim] || !rawExtents[dim])
        continue;
      int64_t extent = *rawExtents[dim];
      if (extent <= 0)
        continue;
      if ((extent + blockExtent - 1) / blockExtent == gridExtent) {
        result[i] = extent;
        used[dim] = true;
        matched = true;
        break;
      }
      if (halo && dim < halo->size()) {
        int64_t widened = extent + 2 * (*halo)[dim];
        if (widened > 0 &&
            (widened + blockExtent - 1) / blockExtent == gridExtent) {
          result[i] = widened;
          used[dim] = true;
          matched = true;
          break;
        }
      }
    }
    if (!matched)
      return std::nullopt; // no distinct committed extent yields this grid
                           // count
  }
  return result;
}

LogicalResult MuLayoutRewriter::apply(SdeMuAllocOp muAlloc) {
  Value oldMemref = muAlloc.getMemref();
  auto logicalType = dyn_cast<MemRefType>(oldMemref.getType());
  if (!logicalType ||
      logicalType.getRank() != static_cast<int64_t>(layout.logicalRank()))
    return failure();

  // Pre-scan: every use must be a direct memref.load / memref.store on the MU
  // root (logical-rank indexed), or a dealloc. Anything else -> fail closed,
  // no mutation.
  llvm::SmallVector<memref::LoadOp, 8> loads;
  llvm::SmallVector<memref::StoreOp, 8> stores;
  llvm::SmallVector<memref::DeallocOp, 2> deallocs;
  llvm::SmallVector<polygeist::Memref2PointerOp, 2> basePointers;
  llvm::SmallVector<SdeArrayLayoutRootOp, 4> provenance;
  for (OpOperand &use : oldMemref.getUses()) {
    Operation *user = use.getOwner();
    if (auto root = dyn_cast<SdeArrayLayoutRootOp>(user)) {
      if (root.getRoot() != oldMemref)
        return failure();
      provenance.push_back(root);
      continue;
    }
    if (auto dealloc = dyn_cast<memref::DeallocOp>(user)) {
      deallocs.push_back(dealloc);
      continue;
    }
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemRef() != oldMemref ||
          load.getIndices().size() != layout.logicalRank())
        return failure();
      loads.push_back(load);
      continue;
    }
    if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemRef() != oldMemref ||
          store.getIndices().size() != layout.logicalRank())
        return failure();
      stores.push_back(store);
      continue;
    }
    // Only layout-invariant base-pointer checks can follow the expanded memref.
    if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(user)) {
      if (isLayoutInvariantBasePointer(m2p)) {
        basePointers.push_back(m2p);
        continue;
      }
    }
    return failure();
  }

  MemRefType expandedType = buildExpandedMuType(logicalType, layout);

  // Build the new expanded mu_alloc in place of the old one (fully static ->
  // no dynamic sizes).
  OpBuilder builder(muAlloc);
  auto newAlloc = SdeMuAllocOp::create(builder, muAlloc.getLoc(), expandedType,
                                       ValueRange{});
  if (IntegerAttr arrayId = muAlloc.getArrayIdAttr())
    newAlloc.setArrayIdAttr(arrayId);
  Value newMemref = newAlloc.getMemref();

  for (SdeArrayLayoutRootOp root : provenance)
    root->setOperand(0, newMemref);

  // Rewrite reads.
  for (memref::LoadOp load : loads) {
    OpBuilder b(load);
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(load.getIndices(), b, load.getLoc());
    auto newLoad =
        memref::LoadOp::create(b, load.getLoc(), newMemref, ValueRange(idx));
    load.getResult().replaceAllUsesWith(newLoad.getResult());
    load.erase();
  }

  // Rewrite writes.
  for (memref::StoreOp store : stores) {
    OpBuilder b(store);
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(store.getIndices(), b, store.getLoc());
    memref::StoreOp::create(b, store.getLoc(), store.getValueToStore(),
                            newMemref, ValueRange(idx));
    store.erase();
  }

  // Preserve allocation identity checks after replacing the MU root.
  for (polygeist::Memref2PointerOp m2p : basePointers) {
    OpBuilder b(m2p);
    auto repl = polygeist::Memref2PointerOp::create(b, m2p.getLoc(),
                                                    m2p.getType(), newMemref);
    m2p.getResult().replaceAllUsesWith(repl.getResult());
    m2p.erase();
  }

  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();

  muAlloc.erase();
  return success();
}

} // namespace mlir::carts::sde
