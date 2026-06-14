///==========================================================================///
/// File: MuLayoutRewriter.cpp
///
/// Implementation of the SDE MU-level rank-expansion rewriter (see
/// MuLayoutRewriter.h).
///==========================================================================///

#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/Utils.h"
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

static std::optional<MuPhysicalLayout>
resolveMuPhysicalLayoutForWriter(MemRefType logicalType, SdeSuIterateOp writer);

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
      queryStructuredClassification(si);
  if (!cls)
    cls = si.getStructuredClassification();
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
      resolveMuPhysicalLayoutForWriter(logicalType, si);
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
  Value writeRoot = findArrayLayoutRoot(si, arrayId, SdeAccessMode::write);
  if (!writeRoot)
    return std::nullopt;
  if (std::optional<CommittedSuPhysicalLayout> committed =
          recoverCommittedPhysicalLayout(si)) {
    LayoutGraphFact fact;
    fact.id = arrayId;
    fact.role = LayoutGraphRole::write;
    fact.layoutKind = ArrayLayoutKind::blockParallel;
    fact.ownerDims = committed->ownerDims;
    fact.blockShape = committed->blockShape;
    return fact;
  }
  return std::nullopt;
}

static std::optional<LayoutGraphFact>
findBlockLayoutFact(SdeSuIterateOp si, int64_t arrayId, LayoutGraphRole role) {
  if (!si)
    return std::nullopt;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(si.getArrayLayoutAttr())) {
    if (fact.id == arrayId && fact.role == role &&
        fact.layoutKind != ArrayLayoutKind::replicated &&
        !fact.ownerDims.empty() && !fact.blockShape.empty())
      return fact;
  }
  return std::nullopt;
}

static std::optional<LayoutGraphFact>
findSingleWriteBlockLayoutFact(SdeSuIterateOp si) {
  if (!si)
    return std::nullopt;
  std::optional<LayoutGraphFact> selected;
  for (const LayoutGraphFact &fact :
       parseArrayLayoutFacts(si.getArrayLayoutAttr())) {
    if (fact.role != LayoutGraphRole::write ||
        fact.layoutKind != ArrayLayoutKind::blockParallel ||
        fact.ownerDims.empty() || fact.blockShape.empty())
      continue;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->ownerDims != fact.ownerDims ||
        selected->blockShape != fact.blockShape)
      return std::nullopt;
  }
  return selected;
}

static std::optional<MuPhysicalLayout>
resolveMuPhysicalLayoutForWriter(MemRefType logicalType,
                                 SdeSuIterateOp writer) {
  if (!writer)
    return std::nullopt;
  if (std::optional<LayoutGraphFact> fact = findSingleWriteBlockLayoutFact(writer))
    return resolveMuPhysicalLayout(logicalType, fact->ownerDims,
                                   fact->blockShape);
  if (std::optional<CommittedSuPhysicalLayout> committed =
          recoverCommittedPhysicalLayout(writer))
    return resolveMuPhysicalLayout(logicalType, committed->ownerDims,
                                   committed->blockShape);
  return std::nullopt;
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

static SmallVector<int64_t, 4>
ownerDimsAsI64(ArrayRef<unsigned> ownerDims) {
  SmallVector<int64_t, 4> out;
  out.reserve(ownerDims.size());
  for (unsigned dim : ownerDims)
    out.push_back(static_cast<int64_t>(dim));
  return out;
}

static ArrayRef<int64_t> committedBlockShape(const LayoutGraphFact &fact) {
  return fact.budgetBlockShape.empty() ? ArrayRef<int64_t>(fact.blockShape)
                                       : ArrayRef<int64_t>(fact.budgetBlockShape);
}

static std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMuForWriter(SdeSuIterateOp writer,
                                      MemRefType muType) {
  if (!writer || !muType)
    return std::nullopt;
  if (std::optional<LayoutGraphFact> fact = findSingleWriteBlockLayoutFact(writer)) {
    if (std::optional<ExpandedBlockGridMu> expanded =
            recognizeExpandedBlockGridMuFromShape(fact->ownerDims, fact->blockShape,
                                                  muType))
      return expanded;
  }
  if (std::optional<CommittedSuPhysicalLayout> committed =
          recoverCommittedPhysicalLayout(writer))
    if (std::optional<ExpandedBlockGridMu> expanded =
            recognizeExpandedBlockGridMuFromShape(committed->ownerDims,
                                                  committed->blockShape, muType))
      return expanded;
  if (std::optional<RecoveredMuPhysicalLayout> recovered =
          recoverMuPhysicalLayoutFromExpandedType(muType))
    return recognizeExpandedBlockGridMuFromShape(
        ownerDimsAsI64(recovered->ownerDims), recovered->physicalBlockShape,
        muType);
  return std::nullopt;
}

static std::optional<ComparableBlockGrid> resolveComparableBlockGridForWriter(
    SdeSuIterateOp writer, const LayoutGraphFact &fact, MemRefType muType) {
  if (!muType)
    return std::nullopt;
  if (std::optional<MuPhysicalLayout> flat =
          resolveMuPhysicalLayoutForWriter(muType, writer))
    return comparableBlockGrid(*flat);
  if (std::optional<ExpandedBlockGridMu> expanded =
          recognizeExpandedBlockGridMuForWriter(writer, muType))
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
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc);
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

SdeSuIterateOp findCommittedBlockLayoutWitness(SdeMuAllocOp muAlloc) {
  if (SdeSuIterateOp writer = findCommittedBlockLayoutWriter(muAlloc))
    return writer;

  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType)
    return SdeSuIterateOp();
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc);
  if (!arrayId)
    return SdeSuIterateOp();

  SdeSuIterateOp result;
  SdeSuIterateOp staticResult;
  SdeSuIterateOp supportedResult;
  SdeSuIterateOp staticSupportedResult;
  std::optional<ComparableBlockGrid> committedLayout;

  for (Operation *user : muAlloc.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || root.getMode() != SdeAccessMode::read ||
        static_cast<int64_t>(root.getArrayId()) != *arrayId)
      continue;
    SdeSuIterateOp reader = root->getParentOfType<SdeSuIterateOp>();
    std::optional<LayoutGraphFact> fact =
        findBlockLayoutFact(reader, *arrayId, LayoutGraphRole::read);
    if (!reader || !fact)
      continue;

    std::optional<ComparableBlockGrid> layout;
    if (std::optional<MuPhysicalLayout> flat =
            resolveMuPhysicalLayout(muType, fact->ownerDims, fact->blockShape))
      layout = comparableBlockGrid(*flat);
    else if (std::optional<ExpandedBlockGridMu> expanded =
                 recognizeExpandedBlockGridMuFromShape(
                     fact->ownerDims, fact->blockShape, muType))
      layout = comparableBlockGrid(*expanded);
    if (!layout)
      continue;

    if (!result) {
      result = reader;
      committedLayout = *layout;
    } else if (!samePhysicalLayout(*committedLayout, *layout)) {
      return SdeSuIterateOp();
    }
    if (!staticResult && hasStaticIterationDomain(reader))
      staticResult = reader;
    if (!supportsRankExpandedAccessWindows(reader))
      continue;
    if (!supportedResult)
      supportedResult = reader;
    if (!staticSupportedResult && hasStaticIterationDomain(reader))
      staticSupportedResult = reader;
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
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc);
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
        resolveMuPhysicalLayoutForWriter(muType, writer);
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
  if (result)
    return result;

  std::optional<CommittedMuBlockLayout> readResult;
  std::optional<CommittedMuBlockLayout> staticReadResult;
  std::optional<CommittedMuBlockLayout> supportedReadResult;
  std::optional<CommittedMuBlockLayout> staticSupportedReadResult;
  std::optional<ComparableBlockGrid> readCommittedLayout;

  for (Operation *user : muAlloc.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || root.getMode() != SdeAccessMode::read ||
        static_cast<int64_t>(root.getArrayId()) != *arrayId)
      continue;
    SdeSuIterateOp reader = root->getParentOfType<SdeSuIterateOp>();
    std::optional<LayoutGraphFact> fact =
        findBlockLayoutFact(reader, *arrayId, LayoutGraphRole::read);
    if (!reader || !fact)
      continue;
    std::optional<MuPhysicalLayout> layout =
        resolveMuPhysicalLayout(muType, fact->ownerDims, fact->blockShape);
    if (!layout)
      continue;
    ComparableBlockGrid comparable = comparableBlockGrid(*layout);
    if (!readResult) {
      readResult = CommittedMuBlockLayout{reader, *layout};
      readCommittedLayout = comparable;
    } else if (!samePhysicalLayout(*readCommittedLayout, comparable)) {
      return std::nullopt;
    }

    CommittedMuBlockLayout current{reader, *layout};
    if (!staticReadResult && hasStaticIterationDomain(reader))
      staticReadResult = current;
    if (!supportsRankExpandedAccessWindows(reader))
      continue;
    if (!supportedReadResult)
      supportedReadResult = current;
    if (!staticSupportedReadResult && hasStaticIterationDomain(reader))
      staticSupportedReadResult = current;
  }

  if (staticSupportedReadResult)
    return staticSupportedReadResult;
  if (supportedReadResult)
    return supportedReadResult;
  if (staticReadResult)
    return staticReadResult;
  return readResult;
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
            SdeArrayLayoutRootOp, SdeSuHaloOp, SdeSuReduceScatterOp>(user))
      continue;
    if (isa<affine::AffineReadOpInterface, affine::AffineWriteOpInterface>(
            user))
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
  if (std::optional<ExpandedBlockGridMu> fromWriter =
          recognizeExpandedBlockGridMuForWriter(si, muType))
    return fromWriter;
  return std::nullopt;
}

std::optional<ExpandedBlockGridMu>
recognizeExpandedBlockGridMu(SdeMuAllocOp muAlloc) {
  auto muType = dyn_cast<MemRefType>(muAlloc.getMemref().getType());
  if (!muType)
    return std::nullopt;
  std::optional<int64_t> arrayId = getMuArrayIdFromLayoutRoot(muAlloc);
  if (!arrayId) {
    for (Operation *user : muAlloc.getMemref().getUsers()) {
      if (!isa<memref::StoreOp, affine::AffineWriteOpInterface>(user))
        continue;
      SdeSuIterateOp witness = user->getParentOfType<SdeSuIterateOp>();
      if (!witness || !supportsRankExpandedAccessWindows(witness))
        continue;
      if (std::optional<RecoveredMuPhysicalLayout> recovered =
              recoverMuPhysicalLayoutFromExpandedType(muType))
        return recognizeExpandedBlockGridMuFromShape(
            ownerDimsAsI64(recovered->ownerDims),
            recovered->physicalBlockShape, muType);
    }
    return std::nullopt;
  }

  if (SdeSuIterateOp writer = findCommittedBlockLayoutWriter(muAlloc))
    return recognizeExpandedBlockGridMuForWriter(writer, muType);

  std::optional<ExpandedBlockGridMu> result;
  std::optional<ComparableBlockGrid> committedLayout;
  for (Operation *user : muAlloc.getMemref().getUsers()) {
    auto root = dyn_cast<SdeArrayLayoutRootOp>(user);
    if (!root || static_cast<int64_t>(root.getArrayId()) != *arrayId)
      continue;
    if (root.getMode() != SdeAccessMode::read)
      continue;
    SdeSuIterateOp source = root->getParentOfType<SdeSuIterateOp>();
    std::optional<LayoutGraphFact> fact =
        findBlockLayoutFact(source, *arrayId, LayoutGraphRole::read);
    if (!fact)
      continue;
    std::optional<ExpandedBlockGridMu> expanded =
        recognizeExpandedBlockGridMuFromShape(fact->ownerDims,
                                              committedBlockShape(*fact),
                                              muType);
    if (!expanded)
      continue;
    ComparableBlockGrid comparable = comparableBlockGrid(*expanded);
    if (!result) {
      result = *expanded;
      committedLayout = comparable;
    } else if (!samePhysicalLayout(*committedLayout, comparable)) {
      return std::nullopt;
    }
  }
  if (result)
    return result;
  if (std::optional<RecoveredMuPhysicalLayout> recovered =
          recoverMuPhysicalLayoutFromExpandedType(muType))
    return recognizeExpandedBlockGridMuFromShape(
        ownerDimsAsI64(recovered->ownerDims), recovered->physicalBlockShape,
        muType);
  return std::nullopt;
}

std::optional<llvm::SmallVector<int64_t, 4>>
findOwnerIterationExtents(SdeSuIterateOp si,
                          llvm::ArrayRef<int64_t> blockExtents,
                          llvm::ArrayRef<int64_t> gridCounts) {
  if (!si || blockExtents.size() != gridCounts.size() || blockExtents.empty())
    return std::nullopt;
  std::optional<llvm::SmallVector<int64_t, 4>> halo =
      deriveCommittedHaloShape(si);

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
  llvm::SmallVector<affine::AffineReadOpInterface, 8> affineReads;
  llvm::SmallVector<affine::AffineWriteOpInterface, 8> affineWrites;
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
    if (auto read = dyn_cast<affine::AffineReadOpInterface>(user)) {
      if (read.getMemRef() != oldMemref ||
          read.getAffineMap().getNumResults() != layout.logicalRank())
        return failure();
      affineReads.push_back(read);
      continue;
    }
    if (auto write = dyn_cast<affine::AffineWriteOpInterface>(user)) {
      if (write.getMemRef() != oldMemref ||
          write.getAffineMap().getNumResults() != layout.logicalRank())
        return failure();
      affineWrites.push_back(write);
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

  for (affine::AffineReadOpInterface read : affineReads) {
    OpBuilder b(read.getOperation());
    std::optional<SmallVector<Value, 8>> expanded = affine::expandAffineMap(
        b, read->getLoc(), read.getAffineMap(), read.getMapOperands());
    if (!expanded)
      return failure();
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(*expanded, b, read->getLoc());
    auto newLoad =
        memref::LoadOp::create(b, read->getLoc(), newMemref, ValueRange(idx));
    read.getValue().replaceAllUsesWith(newLoad.getResult());
    read->erase();
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

  for (affine::AffineWriteOpInterface write : affineWrites) {
    OpBuilder b(write.getOperation());
    std::optional<SmallVector<Value, 8>> expanded = affine::expandAffineMap(
        b, write->getLoc(), write.getAffineMap(), write.getMapOperands());
    if (!expanded)
      return failure();
    llvm::SmallVector<Value, 6> idx =
        indexer.localize(*expanded, b, write->getLoc());
    memref::StoreOp::create(b, write->getLoc(), write.getValueToStore(),
                            newMemref, ValueRange(idx));
    write->erase();
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
