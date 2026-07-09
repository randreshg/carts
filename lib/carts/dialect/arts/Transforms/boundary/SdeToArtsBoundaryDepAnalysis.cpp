///==========================================================================///
/// File: SdeToArtsBoundaryDepAnalysis.cpp
/// SDE access-window and dependency analysis for ARTS realization.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/utils/Numeric.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
#include "llvm/ADT/DenseMap.h"
#include <algorithm>
#include <functional>
#include <limits>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromSuIterateAttrs(sde::SdeSuIterateOp source) {
  if (!source || source.getBody().empty())
    return std::nullopt;

  std::optional<CommittedPhysicalLayout> selected;
  auto record = [&](ArrayRef<int64_t> ownerDims,
                    ArrayRef<int64_t> blockShape) -> bool {
    if (ownerDims.empty() || blockShape.empty())
      return true;
    CommittedPhysicalLayout candidate{
        SmallVector<int64_t, 4>(ownerDims.begin(), ownerDims.end()),
        SmallVector<int64_t, 4>(blockShape.begin(), blockShape.end())};
    if (!selected) {
      selected = std::move(candidate);
      return true;
    }
    return selected->ownerDims == candidate.ownerDims &&
           selected->blockShape == candidate.blockShape;
  };

  Block &entry = source.getBody().front();
  for (sde::SdeArrayLayoutOp layout : entry.getOps<sde::SdeArrayLayoutOp>()) {
    if (layout.getMode() != sde::SdeAccessMode::write)
      continue;
    DenseI64ArrayAttr ownerDims = layout.getOwnerDimsAttr();
    if (!ownerDims)
      return std::nullopt;
    if (!record(ownerDims.asArrayRef(), layout.getBlockShapeAttr().asArrayRef()))
      return std::nullopt;
  }

  for (const sde::LayoutGraphFact &fact :
       sde::parseArrayLayoutFacts(source.getArrayLayoutAttr())) {
    if (fact.role != sde::LayoutGraphRole::write)
      continue;
    if (!record(fact.ownerDims, fact.blockShape))
      return std::nullopt;
  }

  return selected;
}

std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromSuIterateOwnerFacts(sde::SdeSuIterateOp source) {
  return readPhysicalLayoutFromSuIterateAttrs(source);
}

std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromDeps(ArrayRef<DirectDepSpec> deps) {
  std::optional<CommittedPhysicalLayout> selected;
  for (const DirectDepSpec &dep : deps) {
    if (!dep.arrayOwnerDims || dep.arrayOwnerDims->empty() ||
        dep.validExtents.empty())
      continue;
    if (dep.arrayOwnerDims->size() != dep.ownerDimCount)
      return std::nullopt;

    if (!selected) {
      selected = CommittedPhysicalLayout{
          SmallVector<int64_t, 4>(dep.arrayOwnerDims->begin(),
                                  dep.arrayOwnerDims->end()),
          SmallVector<int64_t, 4>(dep.validExtents.begin(),
                                  dep.validExtents.end())};
      continue;
    }

    if (selected->ownerDims != *dep.arrayOwnerDims ||
        selected->blockShape.size() != dep.validExtents.size())
      return std::nullopt;
    for (auto [slot, extent] : llvm::enumerate(dep.validExtents)) {
      int64_t &selectedExtent = selected->blockShape[slot];
      if (selectedExtent == extent)
        continue;
      int64_t lo = std::min(selectedExtent, extent);
      int64_t hi = std::max(selectedExtent, extent);
      if (lo <= 0 || hi % lo != 0)
        return std::nullopt;
      selectedExtent = hi;
    }
  }
  return selected;
}

std::optional<CommittedPhysicalLayout>
readCommittedPhysicalLayout(sde::SdeSuIterateOp source,
                            ArrayRef<DirectDepSpec> deps) {
  if (std::optional<CommittedPhysicalLayout> fromAttrs =
          readPhysicalLayoutFromSuIterateOwnerFacts(source))
    return fromAttrs;
  if (std::optional<CommittedPhysicalLayout> fromDeps =
          readPhysicalLayoutFromDeps(deps))
    return fromDeps;
  return std::nullopt;
}

bool hasDistributedLaunchStorageFacts(ArrayRef<DirectDepSpec> deps) {
  return llvm::any_of(deps, [](DirectDepSpec dep) {
    return dep.alloc && hasArtsDbPhysicalLayout(dep.alloc.getOperation());
  });
}

bool hasDistributedWriterStorageFacts(ArrayRef<DirectDepSpec> deps) {
  return llvm::any_of(deps, [](DirectDepSpec dep) {
    return dep.alloc && arts::DbUtils::isWriterMode(dep.mode) &&
           hasArtsDbPhysicalLayout(dep.alloc.getOperation());
  });
}

bool accessModeCovers(ArtsMode available, ArtsMode requested) {
  if (requested == ArtsMode::uninitialized)
    return true;
  if (available == ArtsMode::inout)
    return requested == ArtsMode::in || requested == ArtsMode::out ||
           requested == ArtsMode::inout;
  return available == requested;
}

std::optional<unsigned>
findDirectDepIndexForAccess(ArrayRef<DirectDepSpec> deps, arts::DbAllocOp alloc,
                            ArtsMode mode, bool preferHaloRead) {
  std::optional<unsigned> fallback;
  for (auto [depIdx, dep] : llvm::enumerate(deps)) {
    if (dep.alloc != alloc || !accessModeCovers(dep.mode, mode))
      continue;
    unsigned index = static_cast<unsigned>(depIdx);
    if (mode == ArtsMode::in && preferHaloRead && dep.haloShape)
      return index;
    if (!fallback)
      fallback = index;
  }
  return fallback;
}

bool containsI64(ArrayRef<int64_t> values, int64_t needle) {
  return llvm::is_contained(values, needle);
}

int64_t saturatedMul(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

int64_t ceilDivPositiveI64(int64_t lhs, int64_t rhs) {
  if (lhs <= 0 || rhs <= 0)
    return 0;
  return carts::ceilDivPositive(lhs, rhs);
}

FailureOr<ArrayAttr>
buildPartialReductionDepResultDimMap(sde::SdeSuIterateOp source,
                                     const DirectDepSpec &dep) {
  MLIRContext *ctx = source.getContext();
  auto emptyMap = [&]() -> ArrayAttr { return Builder(ctx).getArrayAttr({}); };
  if (!hasCommittedPartialReductionFacts(source))
    return emptyMap();

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(source.getPartialReductionOwnerDimsAttr());
  std::optional<SmallVector<int64_t, 4>> reductionDims =
      readI64ArrayAttr(source.getPartialReductionDimsAttr());
  if (!ownerDims || ownerDims->empty() || !reductionDims ||
      reductionDims->empty())
    return source.emitOpError()
           << "commits partial reduction without concrete owner/reduction dims";

  if (dep.reduceScatter) {
    if (dep.mode != ArtsMode::in)
      return source.emitOpError()
             << "matched reduce_scatter_like dependency is not read-only";
    if (reductionDims->size() != 1)
      return source.emitOpError() << "direct reduce_scatter_like ARTS map "
                                     "currently requires exactly "
                                     "one committed reduction-only dimension";
    return buildI64ArrayAttr(ctx, SmallVector<int64_t, 1>{-1});
  }

  SmallVector<int64_t, 4> depOwnerDims = makeAllDbOwnerDims(dep.ownerDimCount);
  if (dep.mode != ArtsMode::in) {
    for (int64_t dim : *ownerDims)
      if (!containsI64(depOwnerDims, dim))
        return source.emitOpError()
               << "partial-reduction result dependency does not cover all "
                  "committed owner dims";
    return buildI64ArrayAttr(ctx, *ownerDims);
  }

  if (depOwnerDims.empty())
    return emptyMap();

  SmallVector<int64_t, 4> mappedDims;
  for (int64_t dim : depOwnerDims)
    if (containsI64(*ownerDims, dim))
      mappedDims.push_back(dim);

  return buildI64ArrayAttr(ctx, mappedDims);
}

bool areWriterGroupsRouteLocal(ArrayRef<WriterGroupingSpec> writerSpecs,
                               ArrayRef<int64_t> counts, int64_t totalNodes) {
  return llvm::all_of(writerSpecs, [&](const WriterGroupingSpec &spec) {
    SmallVector<int64_t, 4> projectedCounts(spec.dbSizes.size(), 1);
    if (spec.groupSlotByDbDim.empty()) {
      if (counts.size() != spec.dbSizes.size())
        return false;
      projectedCounts.assign(counts.begin(), counts.end());
    } else {
      if (spec.groupSlotByDbDim.size() != spec.dbSizes.size())
        return false;
      if (spec.globalBlockSizeByDbDim.size() != spec.dbSizes.size() ||
          spec.coordinateBlockSizeByDbDim.size() != spec.dbSizes.size())
        return false;
      for (auto [dbDim, rawGroupSlot] :
           llvm::enumerate(spec.groupSlotByDbDim)) {
        if (rawGroupSlot < 0)
          continue;
        if (static_cast<size_t>(rawGroupSlot) >= counts.size())
          return false;
        int64_t globalBlockSize = spec.globalBlockSizeByDbDim[dbDim];
        int64_t coordinateBlockSize = spec.coordinateBlockSizeByDbDim[dbDim];
        if (globalBlockSize <= 0 || coordinateBlockSize <= 0)
          return false;
        projectedCounts[dbDim] = ceilDivPositiveI64(
            saturatedMul(counts[rawGroupSlot], globalBlockSize),
            coordinateBlockSize);
      }
    }
    return isStaticDbOwnerGroupedBlockScheduleRouteLocal(
        spec.dbSizes, projectedCounts, totalNodes, spec.ownerFacts);
  });
}

std::optional<unsigned>
mapOwnerSlotToAccessWindowPayloadDim(unsigned ownerSlot, int64_t physicalDim,
                                     unsigned ownerDimCount,
                                     ArrayRef<int64_t> validExtents) {
  if (physicalDim < 0)
    return std::nullopt;
  if (validExtents.size() == ownerDimCount) {
    if (ownerSlot >= validExtents.size())
      return std::nullopt;
    return ownerSlot;
  }
  if (static_cast<size_t>(physicalDim) >= validExtents.size())
    return std::nullopt;
  return static_cast<unsigned>(physicalDim);
}

FailureOr<unsigned> getAccessWindowPayloadDim(sde::SdeSuIterateOp source,
                                              const DirectDepSpec &dep,
                                              unsigned ownerSlot,
                                              unsigned dispatchPhysicalDim) {
  if (dep.arrayOwnerDims) {
    if (ownerSlot >= dep.arrayOwnerDims->size())
      return source.emitOpError()
             << "dependency array owner-dim facts do not cover owner slot";
    int64_t physicalDim = (*dep.arrayOwnerDims)[ownerSlot];
    std::optional<unsigned> payloadDim = mapOwnerSlotToAccessWindowPayloadDim(
        ownerSlot, physicalDim, dep.ownerDimCount, dep.validExtents);
    if (!payloadDim)
      return source.emitOpError()
             << "dependency array owner dimension is outside the "
                "access-window payload rank";
    return *payloadDim;
  }

  if (dep.validExtents.size() == dep.ownerDimCount) {
    if (ownerSlot >= dep.validExtents.size())
      return source.emitOpError()
             << "access-window valid extent rank does not cover owner slot";
    return ownerSlot;
  }

  if (dispatchPhysicalDim >= dep.validExtents.size())
    return source.emitOpError()
           << "access-window valid extent rank does not cover dispatch owner "
              "dimension";
  return dispatchPhysicalDim;
}

FailureOr<int64_t> getAccessWindowPayloadExtent(sde::SdeSuIterateOp source,
                                                const DirectDepSpec &dep,
                                                unsigned depPayloadDim) {
  DbAllocOp alloc = dep.alloc;
  if (!alloc || depPayloadDim >= dep.validExtents.size())
    return source.emitOpError()
           << "access-window valid extent rank does not cover physical owner "
              "dimension";

  int64_t payloadExtent = dep.validExtents[depPayloadDim];
  if (payloadExtent <= 0)
    return source.emitOpError()
           << "access-window payload extent must be positive for direct ARTS "
              "dispatch";

  unsigned payloadDim = dep.ownerDimCount + depPayloadDim;
  if (alloc.getElementSizes().size() == dep.validExtents.size())
    payloadDim = depPayloadDim;
  if (payloadDim >= alloc.getElementSizes().size())
    return source.emitOpError()
           << "rank-expanded DB payload shape does not cover access-window "
              "physical dimension";

  std::optional<int64_t> dbPayloadExtent = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(alloc.getElementSizes()[payloadDim]));
  if (!dbPayloadExtent || *dbPayloadExtent != payloadExtent)
    return source.emitOpError()
           << "rank-expanded DB payload extent disagrees with SDE "
              "access-window block extent";

  return payloadExtent;
}

std::optional<SmallVector<int64_t, 4>>
findLargestOwnerLocalGroupCounts(ArrayRef<WriterGroupingSpec> writerSpecs,
                                 ArrayRef<int64_t> requestedCounts,
                                 int64_t totalNodes) {
  if (writerSpecs.empty() || requestedCounts.empty())
    return std::nullopt;
  if (areWriterGroupsRouteLocal(writerSpecs, requestedCounts, totalNodes))
    return SmallVector<int64_t, 4>(requestedCounts.begin(),
                                   requestedCounts.end());

  SmallVector<int64_t, 4> upperCounts(requestedCounts.begin(),
                                      requestedCounts.end());
  for (const WriterGroupingSpec &spec : writerSpecs) {
    if (spec.groupSlotByDbDim.empty()) {
      if (spec.dbSizes.size() != upperCounts.size())
        return std::nullopt;
      for (auto &&[upper, dbSize] : llvm::zip_equal(upperCounts, spec.dbSizes))
        upper = std::min(upper, dbSize);
      continue;
    }
    if (spec.groupSlotByDbDim.size() != spec.dbSizes.size())
      return std::nullopt;
    if (spec.globalBlockSizeByDbDim.size() != spec.dbSizes.size() ||
        spec.coordinateBlockSizeByDbDim.size() != spec.dbSizes.size())
      return std::nullopt;
    for (auto [dbDim, values] : llvm::enumerate(
             llvm::zip_equal(spec.dbSizes, spec.groupSlotByDbDim))) {
      auto [dbSize, rawGroupSlot] = values;
      if (rawGroupSlot < 0)
        continue;
      if (static_cast<size_t>(rawGroupSlot) >= upperCounts.size())
        return std::nullopt;
      int64_t globalBlockSize = spec.globalBlockSizeByDbDim[dbDim];
      int64_t coordinateBlockSize = spec.coordinateBlockSizeByDbDim[dbDim];
      if (globalBlockSize <= 0 || coordinateBlockSize <= 0)
        return std::nullopt;
      int64_t globalCountLimit = ceilDivPositiveI64(
          saturatedMul(dbSize, coordinateBlockSize), globalBlockSize);
      upperCounts[rawGroupSlot] =
          std::min(upperCounts[rawGroupSlot], globalCountLimit);
    }
  }

  SmallVector<int64_t, 4> suffixMax(upperCounts.size() + 1, 1);
  for (int64_t dim = static_cast<int64_t>(upperCounts.size()) - 1; dim >= 0;
       --dim)
    suffixMax[dim] = saturatedMul(upperCounts[dim], suffixMax[dim + 1]);

  SmallVector<int64_t, 4> current(upperCounts.size(), 1);
  SmallVector<int64_t, 4> best;
  int64_t bestScore = 0;
  std::function<void(unsigned, int64_t)> visit = [&](unsigned dim,
                                                     int64_t prefixScore) {
    if (saturatedMul(prefixScore, suffixMax[dim]) <= bestScore)
      return;
    if (dim == upperCounts.size()) {
      if (!areWriterGroupsRouteLocal(writerSpecs, current, totalNodes))
        return;
      if (prefixScore > bestScore) {
        bestScore = prefixScore;
        best.assign(current.begin(), current.end());
      }
      return;
    }
    for (int64_t count = upperCounts[dim]; count >= 1; --count) {
      current[dim] = count;
      visit(dim + 1, saturatedMul(prefixScore, count));
    }
  };
  visit(/*dim=*/0, /*prefixScore=*/1);
  if (best.empty())
    return std::nullopt;
  return best;
}

LogicalResult ensureDistributedWriterOwnerLocalGroups(
    sde::SdeSuIterateOp source, ArrayRef<DirectDepSpec> deps,
    SmallVectorImpl<int64_t> &groupBlockCounts,
    SmallVectorImpl<int64_t> &workerSpans, ArrayRef<int64_t> ownerBlockSizes,
    ArrayRef<int64_t> dispatchedOwnerDims,
    ArrayRef<unsigned> dispatchedLoopDims, int64_t totalNodes,
    bool &splitToOwnerLocalGroups) {
  splitToOwnerLocalGroups = false;
  int64_t validationNodes = std::max<int64_t>(totalNodes, 2);
  if (!llvm::any_of(groupBlockCounts, [](int64_t count) { return count > 1; }))
    return success();

  auto findGlobalOwnerSlot = [&](int64_t ownerDim) -> std::optional<unsigned> {
    auto it = llvm::find(dispatchedOwnerDims, ownerDim);
    if (it == dispatchedOwnerDims.end())
      return std::nullopt;
    return static_cast<unsigned>(
        std::distance(dispatchedOwnerDims.begin(), it));
  };
  auto findGlobalAccessSlot = [&](const DirectDepSpec &dep,
                                  unsigned depSlot) -> std::optional<unsigned> {
    if (depSlot < dep.accessSlots.size()) {
      const DepOwnerAccessSlot &access = dep.accessSlots[depSlot];
      if (access.loopDim) {
        auto loopIt = llvm::find(dispatchedLoopDims, *access.loopDim);
        if (loopIt == dispatchedLoopDims.end())
          return std::nullopt;
        return static_cast<unsigned>(
            std::distance(dispatchedLoopDims.begin(), loopIt));
      }
    }
    int64_t physicalDim =
        dep.arrayOwnerDims && depSlot < dep.arrayOwnerDims->size()
            ? (*dep.arrayOwnerDims)[depSlot]
            : static_cast<int64_t>(depSlot);
    return findGlobalOwnerSlot(physicalDim);
  };

  SmallVector<WriterGroupingSpec, 4> writerSpecs;
  for (const DirectDepSpec &dep : deps) {
    arts::DbAllocOp alloc = dep.alloc;
    if (!alloc || !arts::DbUtils::isWriterMode(dep.mode) ||
        !hasArtsDbPhysicalLayout(alloc.getOperation()))
      continue;

    std::optional<DbOwnerRouteFacts> ownerFacts =
        deriveDbOwnerRouteFactsFromDbGrid(alloc);
    if (!ownerFacts)
      return source.emitOpError()
             << "commits logicalWorkerSlice whose grouped distributed writer "
                "range cannot be proven owner-local from the DB block grid";

    SmallVector<Value, 4> dbSizeValues(alloc.getSizes().begin(),
                                       alloc.getSizes().end());
    std::optional<SmallVector<int64_t, 4>> dbSizes =
        foldStaticDbIndexValues(dbSizeValues);
    if (!dbSizes)
      return source.emitOpError()
             << "commits logicalWorkerSlice whose grouped distributed writer "
                "range cannot be proven owner-local from static DB block-grid "
                "facts";

    SmallVector<int64_t, 4> groupSlotByDbDim(dbSizes->size(), -1);
    SmallVector<int64_t, 4> globalBlockSizeByDbDim(dbSizes->size(), 0);
    SmallVector<int64_t, 4> coordinateBlockSizeByDbDim(dbSizes->size(), 0);
    unsigned ownerSlotCount =
        std::min<unsigned>(dep.ownerDimCount, groupSlotByDbDim.size());
    for (unsigned depSlot = 0; depSlot < ownerSlotCount; ++depSlot) {
      std::optional<unsigned> globalSlot = findGlobalAccessSlot(dep, depSlot);
      if (!globalSlot)
        return source.emitOpError()
               << "commits logicalWorkerSlice whose grouped distributed "
                  "writer range cannot be mapped to a dispatched owner slot";
      groupSlotByDbDim[depSlot] = static_cast<int64_t>(*globalSlot);
      if (*globalSlot >= ownerBlockSizes.size())
        return source.emitOpError()
               << "commits logicalWorkerSlice whose grouped distributed "
                  "writer range references an invalid owner block slot";
      int64_t coordinateBlockSize =
          dep.accessSlots[depSlot].coordinateBlockSize;
      for (auto [ownerSlot, rawDim] : llvm::enumerate(ownerFacts->dims))
        if (rawDim == static_cast<int64_t>(depSlot) &&
            ownerSlot < ownerFacts->blockShape.size())
          coordinateBlockSize =
              std::max(coordinateBlockSize, ownerFacts->blockShape[ownerSlot]);
      if (coordinateBlockSize <= 0)
        return source.emitOpError()
               << "commits logicalWorkerSlice whose grouped distributed "
                  "writer range has a non-positive block size";
      globalBlockSizeByDbDim[depSlot] = ownerBlockSizes[*globalSlot];
      coordinateBlockSizeByDbDim[depSlot] = coordinateBlockSize;
    }

    writerSpecs.push_back({*dbSizes, std::move(groupSlotByDbDim),
                           std::move(globalBlockSizeByDbDim),
                           std::move(coordinateBlockSizeByDbDim), *ownerFacts});
  }

  std::optional<SmallVector<int64_t, 4>> ownerLocalCounts =
      findLargestOwnerLocalGroupCounts(writerSpecs, groupBlockCounts,
                                       validationNodes);
  if (!ownerLocalCounts)
    return source.emitOpError()
           << "cannot split grouped distributed writer into proven "
              "owner-local block ranges from the DB block grid";

  if (sameI64Values(*ownerLocalCounts, groupBlockCounts))
    return success();

  if (ownerBlockSizes.size() != groupBlockCounts.size())
    return source.emitOpError()
           << "cannot split distributed writer grouping because physical "
              "owner block rank does not match logicalWorkerSlice rank";

  groupBlockCounts.assign(ownerLocalCounts->begin(), ownerLocalCounts->end());
  for (auto [span, blockSize, count] :
       llvm::zip_equal(workerSpans, ownerBlockSizes, groupBlockCounts))
    span = blockSize * count;
  splitToOwnerLocalGroups = true;

  return success();
}

bool hasDistributedLaunchStorageFacts(ArrayRef<DirectCuDepSpec> deps) {
  return llvm::any_of(deps, [](DirectCuDepSpec dep) {
    return dep.alloc && hasArtsDbPhysicalLayout(dep.alloc.getOperation());
  });
}

FailureOr<AccessWindowFacts> getAccessWindowFacts(arts::DbAccessWindowOp window,
                                                  unsigned ownerDimCount) {
  std::optional<SmallVector<int64_t, 4>> blockLo =
      readI64ArrayAttr(window.getBlockLo());
  std::optional<SmallVector<int64_t, 4>> blockHi =
      readI64ArrayAttr(window.getBlockHi());
  std::optional<SmallVector<int64_t, 4>> validExtents =
      readI64ArrayAttr(window.getValidExtents());
  if (!blockLo || !blockHi || !validExtents ||
      blockLo->size() != ownerDimCount || blockHi->size() != ownerDimCount) {
    window.emitOpError()
        << "has access-window evidence inconsistent with ownerDimCount";
    return failure();
  }
  for (auto [lo, hi] : llvm::zip(*blockLo, *blockHi))
    if (hi <= lo) {
      window.emitOpError() << "has empty or inverted block window";
      return failure();
    }
  for (int64_t extent : *validExtents)
    if (extent <= 0) {
      window.emitOpError() << "has non-positive valid extent";
      return failure();
    }
  return AccessWindowFacts{
      SmallVector<int64_t, 4>(blockLo->begin(), blockLo->end()),
      SmallVector<int64_t, 4>(blockHi->begin(), blockHi->end()),
      SmallVector<int64_t, 4>(validExtents->begin(), validExtents->end())};
}

bool accessWindowRoleMatches(sde::LayoutGraphRole role, ArtsMode mode) {
  if (mode == ArtsMode::in)
    return role == sde::LayoutGraphRole::read;
  if (arts::DbUtils::isWriterMode(mode))
    return role == sde::LayoutGraphRole::write;
  return false;
}

FailureOr<std::optional<SmallVector<int64_t, 4>>>
getArrayOwnerDimsForWindow(sde::SdeSuIterateOp source,
                           arts::DbAccessWindowOp window, ArtsMode mode,
                           const AccessWindowFacts &facts) {
  IntegerAttr arrayId = window.getArrayIdAttr();
  if (!arrayId)
    return std::optional<SmallVector<int64_t, 4>>{};

  auto verifyOwnerDims = [&](ArrayRef<int64_t> ownerDims)
      -> FailureOr<std::optional<SmallVector<int64_t, 4>>> {
    if (ownerDims.size() != static_cast<size_t>(window.getOwnerDimCount()))
      return window.emitOpError()
             << "owner-dim count disagrees with committed SDE layout fact";

    unsigned ownerDimCount = static_cast<unsigned>(window.getOwnerDimCount());
    for (auto [slot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (!mapOwnerSlotToAccessWindowPayloadDim(static_cast<unsigned>(slot),
                                                ownerDim, ownerDimCount,
                                                facts.validExtents))
        return window.emitOpError()
               << "committed SDE layout owner dimension is outside the "
                  "access-window payload rank";
    }

    return std::optional<SmallVector<int64_t, 4>>{
        SmallVector<int64_t, 4>(ownerDims.begin(), ownerDims.end())};
  };

  std::optional<sde::SdeArrayLayoutOp> typedMatch;
  if (!source.getBody().empty()) {
    for (sde::SdeArrayLayoutOp layout :
         source.getBody().front().getOps<sde::SdeArrayLayoutOp>()) {
      if (static_cast<int64_t>(layout.getArrayId()) != arrayId.getInt() ||
          layout.getMode() != (mode == ArtsMode::in ? sde::SdeAccessMode::read
                                                    : sde::SdeAccessMode::write))
        continue;
      if (typedMatch)
        return window.emitOpError()
               << "matches multiple committed SDE layout facts";
      typedMatch = layout;
    }
  }
  if (typedMatch) {
    DenseI64ArrayAttr ownerDims = (*typedMatch).getOwnerDimsAttr();
    if (!ownerDims)
      return window.emitOpError()
             << "committed SDE layout fact has dynamic owner dimensions; "
                "SDE must materialize static owner dims before ARTS lowering";
    return verifyOwnerDims(ownerDims.asArrayRef());
  }

  ArrayAttr layout = source.getArrayLayoutAttr();
  if (!layout)
    return window.emitOpError()
           << "has SDE access-window arrayId without a committed SDE layout "
              "fact";

  std::optional<sde::LayoutGraphFact> match;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.id != arrayId.getInt() ||
        !accessWindowRoleMatches(fact.role, mode))
      continue;
    if (match)
      return window.emitOpError()
             << "matches multiple committed SDE arrayLayout facts";
    match = fact;
  }

  if (!match)
    return window.emitOpError()
           << "has SDE access-window arrayId without a committed SDE layout "
              "fact";
  return verifyOwnerDims(match->ownerDims);
}

FailureOr<SmallVector<int64_t, 4>>
getReduceScatterOwnerDimsForWindow(arts::DbAccessWindowOp window,
                                   const AccessWindowFacts &facts,
                                   const ReduceScatterRedistFacts &redist) {
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(redist.sourceOwnerDims);
  if (!ownerDims || ownerDims->empty())
    return window.emitOpError()
           << "matches reduce_scatter_like movement without concrete owner "
              "dimensions";
  if (ownerDims->size() != static_cast<size_t>(window.getOwnerDimCount()))
    return window.emitOpError()
           << "owner-dim count disagrees with committed reduce_scatter_like "
              "movement";
  unsigned ownerDimCount = static_cast<unsigned>(window.getOwnerDimCount());
  for (auto [slot, ownerDim] : llvm::enumerate(*ownerDims)) {
    if (!mapOwnerSlotToAccessWindowPayloadDim(static_cast<unsigned>(slot),
                                              ownerDim, ownerDimCount,
                                              facts.validExtents))
      return window.emitOpError()
             << "reduce_scatter_like owner dimension is outside the "
                "access-window payload rank";
  }
  return SmallVector<int64_t, 4>(ownerDims->begin(), ownerDims->end());
}

std::optional<unsigned>
findSingleMappedLoopDim(Value value, ArrayRef<MappedLoopIv> mappedIvs) {
  std::optional<unsigned> selected;
  for (const MappedLoopIv &mapped : mappedIvs) {
    if (!mapped.loopDim)
      continue;
    if (!ValueAnalysis::sameValue(value, mapped.iv) &&
        !ValueAnalysis::dependsOn(value, mapped.iv))
      continue;
    if (selected && *selected != *mapped.loopDim)
      return std::nullopt;
    selected = *mapped.loopDim;
  }
  return selected;
}

SmallVector<MappedLoopIv, 8> collectMappedLoopIvs(sde::SdeSuIterateOp source,
                                                  Block *computeBlock) {
  SmallVector<MappedLoopIv, 8> mapped;
  if (!source || source.getBody().empty() || !computeBlock)
    return mapped;

  unsigned loopRank = source.getUpperBounds().size();
  Block &body = source.getBody().front();
  for (unsigned dim = 0; dim < loopRank && dim < body.getNumArguments();
       ++dim) {
    mapped.push_back(
        {body.getArgument(dim), dim,
         ValueAnalysis::tryFoldConstantIndex(source.getLowerBounds()[dim]),
         ValueAnalysis::tryFoldConstantIndex(source.getUpperBounds()[dim]),
         ValueAnalysis::tryFoldConstantIndex(source.getSteps()[dim])});
  }

  computeBlock->walk<WalkOrder::PreOrder>([&](scf::ForOp loop) {
    std::optional<unsigned> lowerDim =
        findSingleMappedLoopDim(loop.getLowerBound(), mapped);
    std::optional<unsigned> upperDim =
        findSingleMappedLoopDim(loop.getUpperBound(), mapped);
    std::optional<unsigned> selected = lowerDim ? lowerDim : upperDim;
    if (lowerDim && upperDim && *lowerDim != *upperDim)
      return;
    mapped.push_back({loop.getInductionVar(), selected,
                      ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound()),
                      ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound()),
                      ValueAnalysis::tryFoldConstantIndex(loop.getStep())});
  });

  computeBlock->walk<WalkOrder::PreOrder>([&](affine::AffineForOp loop) {
    mapped.push_back({loop.getInductionVar(), std::nullopt,
                      loop.hasConstantLowerBound()
                          ? std::optional<int64_t>(loop.getConstantLowerBound())
                          : std::nullopt,
                      loop.hasConstantUpperBound()
                          ? std::optional<int64_t>(loop.getConstantUpperBound())
                          : std::nullopt,
                      loop.hasConstantBounds()
                          ? std::optional<int64_t>(loop.getStepAsInt())
                          : std::nullopt});
  });

  return mapped;
}

static std::optional<unsigned> loopDimForIv(Value iv,
                                            ArrayRef<MappedLoopIv> mappedIvs) {
  for (const MappedLoopIv &mapped : mappedIvs) {
    if (mapped.loopDim && ValueAnalysis::sameValue(mapped.iv, iv))
      return *mapped.loopDim;
  }
  return std::nullopt;
}

static std::optional<unsigned>
uniqueReferencedLoopDim(AffineExpr expr, ArrayRef<Value> ivOrder,
                        ArrayRef<MappedLoopIv> mappedIvs) {
  std::optional<unsigned> selected;
  bool conflict = false;
  expr.walk([&](AffineExpr node) {
    if (conflict)
      return;
    auto dim = dyn_cast<AffineDimExpr>(node);
    if (!dim || dim.getPosition() >= ivOrder.size())
      return;
    if (std::optional<unsigned> loopDim =
            loopDimForIv(ivOrder[dim.getPosition()], mappedIvs)) {
      if (selected && *selected != *loopDim)
        conflict = true;
      else if (!selected)
        selected = loopDim;
    }
  });
  return conflict ? std::nullopt : selected;
}

static std::optional<int64_t> matchDimPlusConstant(AffineExpr expr,
                                                   AffineExpr dimExpr) {
  if (expr == dimExpr)
    return 0;
  auto add = dyn_cast<AffineBinaryOpExpr>(expr);
  if (!add || add.getKind() != AffineExprKind::Add)
    return std::nullopt;
  if (auto offset = dyn_cast<AffineConstantExpr>(add.getRHS()))
    if (add.getLHS() == dimExpr)
      return offset.getValue();
  if (auto offset = dyn_cast<AffineConstantExpr>(add.getLHS()))
    if (add.getRHS() == dimExpr)
      return offset.getValue();
  return std::nullopt;
}

static DepOwnerAccessSlot makeLoopAccessSlot(unsigned loopDim,
                                             int64_t coordinateBlockSize,
                                             int64_t elementOffset = 0) {
  DepOwnerAccessSlot slot;
  slot.loopDim = loopDim;
  slot.coordinateBlockSize = coordinateBlockSize;
  slot.minElementOffset = elementOffset;
  slot.maxElementOffset = elementOffset;
  return slot;
}

static bool hasSameAccessSlotIdentity(const DepOwnerAccessSlot &lhs,
                                      const DepOwnerAccessSlot &rhs) {
  return lhs.loopDim == rhs.loopDim &&
         lhs.coordinateBlockSize == rhs.coordinateBlockSize &&
         lhs.fixedBlock == rhs.fixedBlock && lhs.fullWindow == rhs.fullWindow;
}

static LogicalResult
mergeAccessSlotCandidates(Operation *op,
                          SmallVectorImpl<DepOwnerAccessSlot> &selected,
                          ArrayRef<DepOwnerAccessSlot> candidate) {
  if (selected.empty()) {
    selected.assign(candidate.begin(), candidate.end());
    return success();
  }
  if (selected.size() != candidate.size())
    return op->emitError()
           << "uses inconsistent block coordinates for one SDE access window";
  for (auto [selectedSlot, candidateSlot] :
       llvm::zip_equal(selected, candidate)) {
    if (!hasSameAccessSlotIdentity(selectedSlot, candidateSlot))
      return op->emitError()
             << "uses inconsistent block coordinates for one SDE access window";
    selectedSlot.minElementOffset =
        std::min(selectedSlot.minElementOffset, candidateSlot.minElementOffset);
    selectedSlot.maxElementOffset =
        std::max(selectedSlot.maxElementOffset, candidateSlot.maxElementOffset);
  }
  return success();
}

static std::optional<DepOwnerAccessSlot>
analyzeDepOwnerAccessSlotFromAffineExpr(AffineExpr expr,
                                        ArrayRef<MappedLoopIv> mappedIvs,
                                        ArrayRef<Value> ivOrder,
                                        bool allowUnitHaloOffset) {
  expr = simplifyAffineExpr(expr, ivOrder.size(), 0);
  int64_t blockSize = 1;
  while (auto bin = dyn_cast<AffineBinaryOpExpr>(expr)) {
    if (bin.getKind() != AffineExprKind::FloorDiv)
      break;
    auto rhs = dyn_cast<AffineConstantExpr>(bin.getRHS());
    if (!rhs || rhs.getValue() <= 0)
      return std::nullopt;
    blockSize *= rhs.getValue();
    expr = simplifyAffineExpr(bin.getLHS(), ivOrder.size(), 0);
  }

  if (auto cst = dyn_cast<AffineConstantExpr>(expr))
    return DepOwnerAccessSlot{std::nullopt, blockSize, cst.getValue()};

  std::optional<unsigned> loopDim =
      uniqueReferencedLoopDim(expr, ivOrder, mappedIvs);
  if (!loopDim)
    return std::nullopt;

  MLIRContext *ctx = expr.getContext();
  unsigned dimPos = 0;
  bool foundDim = false;
  expr.walk([&](AffineExpr node) {
    if (foundDim)
      return;
    if (auto dim = dyn_cast<AffineDimExpr>(node)) {
      dimPos = dim.getPosition();
      foundDim = true;
    }
  });
  if (!foundDim || dimPos >= ivOrder.size())
    return std::nullopt;
  AffineExpr dimExpr = getAffineDimExpr(dimPos, ctx);

  if (std::optional<int64_t> offset = matchDimPlusConstant(expr, dimExpr)) {
    if (*offset == 0)
      return makeLoopAccessSlot(*loopDim, blockSize);
    if (allowUnitHaloOffset && *offset >= -1 && *offset <= 1)
      return makeLoopAccessSlot(*loopDim, blockSize, *offset);
  }

  if (auto mul = dyn_cast<AffineBinaryOpExpr>(expr)) {
    if (mul.getKind() == AffineExprKind::Mul) {
      int64_t multiplier = 0;
      AffineExpr dimPart;
      if (auto lhs = dyn_cast<AffineConstantExpr>(mul.getLHS())) {
        multiplier = lhs.getValue();
        dimPart = mul.getRHS();
      } else if (auto rhs = dyn_cast<AffineConstantExpr>(mul.getRHS())) {
        multiplier = rhs.getValue();
        dimPart = mul.getLHS();
      }
      if (multiplier > 0 && dimPart == dimExpr && blockSize % multiplier == 0)
        return makeLoopAccessSlot(*loopDim, blockSize / multiplier);
    }
  }

  return std::nullopt;
}

std::optional<DepOwnerAccessSlot>
analyzeDepOwnerAccessIndex(Value rawIndex, ArrayRef<MappedLoopIv> mappedIvs,
                           bool allowUnitHaloOffset = false) {
  Value index = ValueAnalysis::stripNumericCasts(rawIndex);
  if (std::optional<int64_t> fixed = ValueAnalysis::tryFoldConstantIndex(index))
    return DepOwnerAccessSlot{std::nullopt, 1, *fixed};

  SmallVector<Value, 8> ivOrder;
  ivOrder.reserve(mappedIvs.size());
  for (const MappedLoopIv &mapped : mappedIvs) {
    if (mapped.loopDim)
      ivOrder.push_back(mapped.iv);
  }
  if (ivOrder.empty())
    return std::nullopt;

  std::optional<AffineExpr> expr =
      sde::tryGetAffineExpr(index, ivOrder, index.getContext());
  if (!expr)
    return std::nullopt;

  return analyzeDepOwnerAccessSlotFromAffineExpr(*expr, mappedIvs, ivOrder,
                                                 allowUnitHaloOffset);
}

bool accessModeMayUseLoad(ArtsMode mode) {
  return mode == ArtsMode::in || mode == ArtsMode::inout;
}

bool accessModeMayUseStore(ArtsMode mode) {
  return mode == ArtsMode::out || mode == ArtsMode::inout;
}

bool commitsUnitAccessOffset(sde::SdeSuIterateOp source) {
  bool sawOffset = false;
  auto inspect = [&](ArrayAttr offsets) {
    if (!offsets)
      return false;
    for (Attribute attr : offsets) {
      auto intAttr = dyn_cast<IntegerAttr>(attr);
      if (!intAttr)
        return false;
      int64_t value = intAttr.getInt();
      if (value < -1 || value > 1)
        return false;
      sawOffset |= value != 0;
    }
    return true;
  };
  if (!inspect(source.getAccessMinOffsetsAttr()))
    return false;
  if (!inspect(source.getAccessMaxOffsetsAttr()))
    return false;
  return sawOffset;
}

bool isCommittedFullWindowSlot(
    sde::SdeSuIterateOp source, arts::DbAllocOp alloc, unsigned slot,
    ArrayRef<int64_t> blockLo, ArrayRef<int64_t> blockHi,
    ArrayRef<int64_t> validExtents,
    const std::optional<SmallVector<int64_t, 4>> &arrayOwnerDims) {
  if (slot >= blockLo.size() || slot >= blockHi.size() || blockLo[slot] != 0 ||
      blockHi[slot] <= blockLo[slot])
    return false;
  if (slot >= alloc.getSizes().size())
    return false;
  std::optional<int64_t> dbGridExtent =
      ValueAnalysis::tryFoldConstantIndex(alloc.getSizes()[slot]);
  if (!dbGridExtent || blockHi[slot] != *dbGridExtent)
    return false;

  std::optional<SmallVector<int64_t, 4>> physicalOwnerDims =
      readCommittedPhysicalOwnerDims(source, arrayOwnerDims);
  if (!physicalOwnerDims)
    return false;

  int64_t payloadDim = static_cast<int64_t>(slot);
  if (arrayOwnerDims) {
    if (slot >= arrayOwnerDims->size())
      return false;
    std::optional<unsigned> mapped = mapOwnerSlotToAccessWindowPayloadDim(
        slot, (*arrayOwnerDims)[slot], static_cast<unsigned>(blockLo.size()),
        validExtents);
    if (!mapped)
      return false;
    payloadDim = *mapped;
  }
  if (payloadDim < 0 || static_cast<size_t>(payloadDim) >= validExtents.size())
    return false;
  return !llvm::is_contained(*physicalOwnerDims, payloadDim);
}

FailureOr<SmallVector<DepOwnerAccessSlot, 4>> deriveDepOwnerAccessSlots(
    sde::SdeSuIterateOp source, arts::DbAllocOp alloc, ArtsMode mode,
    unsigned ownerDimCount, ArrayRef<int64_t> blockLo,
    ArrayRef<int64_t> blockHi, ArrayRef<int64_t> validExtents,
    const std::optional<SmallVector<int64_t, 4>> &arrayOwnerDims,
    bool allowUnitHaloOffsets = false) {
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  SmallVector<MappedLoopIv, 8> mappedIvs =
      collectMappedLoopIvs(source, computeBlock);

  SmallVector<Value, 8> ivOrder;
  ivOrder.reserve(mappedIvs.size());
  for (const MappedLoopIv &mapped : mappedIvs) {
    if (mapped.loopDim)
      ivOrder.push_back(mapped.iv);
  }

  SmallVector<DepOwnerAccessSlot, 4> selected;
  bool sawAccess = false;
  auto analyzeMapResults = [&](Operation *op, AffineMap map,
                               ValueRange mapOperands)
      -> FailureOr<SmallVector<DepOwnerAccessSlot, 4>> {
    if (!map || map.getNumSymbols() != 0)
      return op->emitError() << "cannot analyze symbolic affine access map for "
                                "SDE access-window "
                                "coordinates";
    if (map.getNumResults() < ownerDimCount)
      return op->emitError()
             << "rank-expanded DB access has fewer block coordinates than its "
                "SDE access window";
    SmallVector<AffineExpr, 4> dimReplacements(map.getNumDims());
    for (auto [dim, operand] : llvm::enumerate(mapOperands)) {
      auto it = llvm::find(ivOrder, operand);
      if (it == ivOrder.end())
        return op->emitError()
               << "cannot map affine access operand to a dispatch loop IV";
      dimReplacements[dim] = getAffineDimExpr(
          static_cast<unsigned>(std::distance(ivOrder.begin(), it)),
          map.getContext());
    }
    SmallVector<DepOwnerAccessSlot, 4> candidate;
    candidate.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      AffineExpr expr = simplifyAffineExpr(
          map.getResult(slot).replaceDimsAndSymbols(dimReplacements, {}),
          ivOrder.size(), 0);
      std::optional<DepOwnerAccessSlot> access =
          analyzeDepOwnerAccessSlotFromAffineExpr(expr, mappedIvs, ivOrder,
                                                  allowUnitHaloOffsets);
      if (!access) {
        if (mode == ArtsMode::in &&
            isCommittedFullWindowSlot(source, alloc, slot, blockLo, blockHi,
                                      validExtents, arrayOwnerDims)) {
          DepOwnerAccessSlot fullWindow;
          fullWindow.fullWindow = true;
          fullWindow.coordinateBlockSize = 1;
          candidate.push_back(fullWindow);
          continue;
        }
        return op->emitError()
               << "cannot map SDE access-window block coordinate to a loop "
                  "dimension";
      }
      candidate.push_back(*access);
    }
    return candidate;
  };

  auto recordIndices = [&](Operation *op, Value memref,
                           ValueRange indices) -> LogicalResult {
    if (resolveBoundaryDbAlloc(memref) != alloc)
      return success();
    sawAccess = true;
    if (indices.size() < ownerDimCount)
      return op->emitError()
             << "rank-expanded DB access has fewer block coordinates than its "
                "SDE access window";
    SmallVector<DepOwnerAccessSlot, 4> candidate;
    candidate.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      std::optional<DepOwnerAccessSlot> access = analyzeDepOwnerAccessIndex(
          indices[slot], mappedIvs, allowUnitHaloOffsets);
      if (!access) {
        if (mode == ArtsMode::in &&
            isCommittedFullWindowSlot(source, alloc, slot, blockLo, blockHi,
                                      validExtents, arrayOwnerDims)) {
          DepOwnerAccessSlot fullWindow;
          fullWindow.fullWindow = true;
          fullWindow.coordinateBlockSize = 1;
          candidate.push_back(fullWindow);
          continue;
        }
        return op->emitError()
               << "cannot map SDE access-window block coordinate to a loop "
                  "dimension";
      }
      candidate.push_back(*access);
    }
    return mergeAccessSlotCandidates(op, selected, candidate);
  };

  WalkResult walk = computeBlock->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (!accessModeMayUseLoad(mode))
        return WalkResult::advance();
      if (failed(recordIndices(op, load.getMemref(), load.getIndices())))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (!accessModeMayUseStore(mode))
        return WalkResult::advance();
      if (failed(recordIndices(op, store.getMemref(), store.getIndices())))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto read = dyn_cast<affine::AffineReadOpInterface>(op)) {
      if (!accessModeMayUseLoad(mode))
        return WalkResult::advance();
      if (resolveBoundaryDbAlloc(read.getMemRef()) != alloc)
        return WalkResult::advance();
      sawAccess = true;
      FailureOr<SmallVector<DepOwnerAccessSlot, 4>> candidate =
          analyzeMapResults(op, read.getAffineMap(), read.getMapOperands());
      if (failed(candidate))
        return WalkResult::interrupt();
      if (failed(mergeAccessSlotCandidates(op, selected, *candidate)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto write = dyn_cast<affine::AffineWriteOpInterface>(op)) {
      if (!accessModeMayUseStore(mode))
        return WalkResult::advance();
      if (resolveBoundaryDbAlloc(write.getMemRef()) != alloc)
        return WalkResult::advance();
      sawAccess = true;
      FailureOr<SmallVector<DepOwnerAccessSlot, 4>> candidate =
          analyzeMapResults(op, write.getAffineMap(), write.getMapOperands());
      if (failed(candidate))
        return WalkResult::interrupt();
      if (failed(mergeAccessSlotCandidates(op, selected, *candidate)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    return WalkResult::advance();
  });
  if (walk.wasInterrupted())
    return failure();
  if (!sawAccess)
    return source.emitOpError()
           << "has an SDE access window with no matching load/store in its CU";
  return selected;
}

LogicalResult collectPrecedingReduceScatterRedists(
    sde::SdeSuIterateOp source,
    DenseMap<Operation *, SmallVector<ReduceScatterRedistFacts, 2>>
        &factsByAlloc,
    SmallVectorImpl<Operation *> &consumedRedists) {
  for (Operation *op = source->getPrevNode(); op; op = op->getPrevNode()) {
    if (auto halo = dyn_cast<sde::SdeSuHaloOp>(op)) {
      (void)halo;
      continue;
    }
    if (auto reduce = dyn_cast<sde::SdeSuReduceScatterOp>(op)) {
      arts::DbAllocOp alloc = resolveBoundaryDbAlloc(reduce.getMu());
      if (!alloc)
        return reduce.emitOpError()
               << "does not reference an ARTS DB-backed MU after storage "
                  "realization";
      FailureOr<ReduceScatterRedistFacts> facts =
          buildReduceScatterRedistFacts(reduce);
      if (failed(facts))
        return failure();
      factsByAlloc[alloc.getOperation()].push_back(*facts);
      consumedRedists.push_back(reduce.getOperation());
      continue;
    }
    break;
  }
  return success();
}

FailureOr<std::optional<ReduceScatterRedistFacts>>
findMatchingReduceScatterFacts(
    arts::DbAccessWindowOp window, arts::DbAllocOp alloc,
    const DenseMap<Operation *, SmallVector<ReduceScatterRedistFacts, 2>>
        &factsByAlloc) {
  auto it = factsByAlloc.find(alloc.getOperation());
  if (it == factsByAlloc.end())
    return std::optional<ReduceScatterRedistFacts>{};

  IntegerAttr windowArrayId = window.getArrayIdAttr();
  if (!windowArrayId)
    return window.emitOpError()
           << "has no arrayId for a committed reduce_scatter_like movement";

  std::optional<ReduceScatterRedistFacts> match;
  for (const ReduceScatterRedistFacts &candidate : it->second) {
    if (!candidate.arrayId ||
        candidate.arrayId.getInt() != windowArrayId.getInt())
      continue;
    if (match)
      return window.emitOpError()
             << "matches multiple committed reduce_scatter_like movements";
    match = candidate;
  }

  if (!match)
    return std::optional<ReduceScatterRedistFacts>{};
  if (window.getMode() != ArtsMode::in)
    return std::optional<ReduceScatterRedistFacts>{};

  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(match->sourceBlockShape);
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(match->sourceOwnerDims);
  std::optional<SmallVector<int64_t, 4>> validExtents =
      readI64ArrayAttr(window.getValidExtents());
  if (!ownerDims || ownerDims->empty() ||
      ownerDims->size() != static_cast<size_t>(window.getOwnerDimCount()))
    return window.emitOpError()
           << "has owner rank incompatible with the committed "
              "reduce_scatter_like movement";
  if (!blockShape || !validExtents ||
      blockShape->size() != ownerDims->size() + validExtents->size())
    return window.emitOpError()
           << "has access-window rank incompatible with the committed "
              "reduce_scatter_like block shape";
  return match;
}

LogicalResult recordCoarseSuAccess(sde::SdeSuIterateOp source, Operation *site,
                                   Value memref, ArtsMode mode,
                                   DenseMap<Operation *, unsigned> &depIndex,
                                   SmallVectorImpl<CoarseSuDependency> &deps) {
  arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
  if (!alloc) {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && isDefinedInside(root, source.getOperation()))
      return success();
    if (isStackScratchMemref(memref))
      return success();
    return site->emitError()
           << "accesses external memref without ARTS DB-backed storage during "
              "coarse SDE-to-ARTS SU realization";
  }

  std::optional<arts::PartitionMode> partitionMode = alloc.getPartitionMode();
  if (!partitionMode || *partitionMode != arts::PartitionMode::coarse)
    return site->emitError()
           << "touches a non-coarse DB without committed SDE access windows; "
              "refusing coarse SU realization";
  if (hasArtsDbPhysicalLayout(alloc.getOperation()) ||
      alloc.getDistributedAttr())
    return site->emitError()
           << "touches a block-grid or distributed DB without committed SDE "
              "access windows; refusing coarse SU realization";

  auto [it, inserted] = depIndex.try_emplace(alloc.getOperation(), deps.size());
  if (inserted) {
    deps.push_back({alloc, mode});
    return success();
  }
  deps[it->second].mode = arts::combineAccessModes(deps[it->second].mode, mode);
  return success();
}

LogicalResult
collectCoarseSuDependencies(sde::SdeSuIterateOp source,
                            SmallVectorImpl<CoarseSuDependency> &deps) {
  DenseMap<Operation *, unsigned> depIndex;
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  WalkResult result = computeBlock->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return failed(recordCoarseSuAccess(source, op, load.getMemref(),
                                         ArtsMode::in, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return failed(recordCoarseSuAccess(source, op, store.getMemref(),
                                         ArtsMode::out, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto copy = dyn_cast<memref::CopyOp>(op)) {
      if (failed(recordCoarseSuAccess(source, op, copy.getSource(),
                                      ArtsMode::in, depIndex, deps)))
        return WalkResult::interrupt();
      if (failed(recordCoarseSuAccess(source, op, copy.getTarget(),
                                      ArtsMode::out, depIndex, deps)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      return failed(recordCoarseSuAccess(source, op, atomic.getAddr(),
                                         ArtsMode::inout, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    return WalkResult::advance();
  });
  return result.wasInterrupted() ? failure() : success();
}

void collectTouchedDbAllocs(sde::SdeSuIterateOp source,
                            DenseSet<Operation *> &touched) {
  source.getBody().walk([&](Operation *op) {
    Value memref;
    if (auto load = dyn_cast<memref::LoadOp>(op))
      memref = load.getMemref();
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      memref = store.getMemref();
    else if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      memref = atomic.getAddr();
    else
      return;
    if (arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref))
      touched.insert(alloc.getOperation());
  });
}

bool canMergeAccessWindowIntoDep(const DirectDepSpec &dep, ArtsMode mode,
                                 ArrayAttr haloShape) {
  bool depIsHaloRead = dep.haloShape != nullptr;
  bool windowIsHaloRead = haloShape != nullptr;
  if (!depIsHaloRead && !windowIsHaloRead)
    return true;
  return depIsHaloRead && windowIsHaloRead && dep.mode == ArtsMode::in &&
         mode == ArtsMode::in;
}

LogicalResult recordAccessWindowDependency(
    sde::SdeSuIterateOp source, arts::DbAccessWindowOp window,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps,
    const DenseMap<Operation *, SmallVector<ReduceScatterRedistFacts, 2>>
        &reduceScatterFactsByAlloc) {
  auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
      arts::DbUtils::getUnderlyingDbAlloc(window.getMu()));
  if (!alloc)
    return window.emitOpError()
           << "does not reference an ARTS DB-backed MU after storage "
              "realization";

  unsigned ownerDimCount = static_cast<unsigned>(window.getOwnerDimCount());
  FailureOr<AccessWindowFacts> facts =
      getAccessWindowFacts(window, ownerDimCount);
  if (failed(facts))
    return failure();
  if (window.getHaloShapeAttr() && window.getMode() != ArtsMode::in)
    return window.emitOpError()
           << "commits a writable halo dependency; SDE must split the read "
              "halo and write access before ARTS realization";
  FailureOr<std::optional<ReduceScatterRedistFacts>> reduceScatter =
      findMatchingReduceScatterFacts(window, alloc, reduceScatterFactsByAlloc);
  if (failed(reduceScatter))
    return failure();
  std::optional<SmallVector<int64_t, 4>> arrayOwnerDims;
  if (reduceScatter->has_value()) {
    FailureOr<SmallVector<int64_t, 4>> redistOwnerDims =
        getReduceScatterOwnerDimsForWindow(window, *facts, **reduceScatter);
    if (failed(redistOwnerDims))
      return failure();
    arrayOwnerDims = std::move(*redistOwnerDims);
  } else {
    FailureOr<std::optional<SmallVector<int64_t, 4>>> layoutOwnerDims =
        getArrayOwnerDimsForWindow(source, window, window.getMode(), *facts);
    if (failed(layoutOwnerDims))
      return failure();
    arrayOwnerDims = std::move(*layoutOwnerDims);
  }
  FailureOr<SmallVector<DepOwnerAccessSlot, 4>> accessSlots =
      deriveDepOwnerAccessSlots(source, alloc, window.getMode(), ownerDimCount,
                                facts->blockLo, facts->blockHi,
                                facts->validExtents, arrayOwnerDims,
                                window.getHaloShapeAttr() != nullptr ||
                                    (window.getMode() == ArtsMode::in &&
                                     commitsUnitAccessOffset(source)));
  if (failed(accessSlots))
    return failure();

  ArrayAttr haloShape;
  if (window.getMode() == ArtsMode::in)
    haloShape = window.getHaloShapeAttr();

  auto appendDep = [&]() {
    DirectDepSpec dep;
    dep.alloc = alloc;
    dep.mode = window.getMode();
    dep.arrayId = window.getArrayIdAttr();
    dep.ownerDimCount = ownerDimCount;
    dep.blockLo.assign(facts->blockLo.begin(), facts->blockLo.end());
    dep.blockHi.assign(facts->blockHi.begin(), facts->blockHi.end());
    dep.validExtents.assign(facts->validExtents.begin(),
                            facts->validExtents.end());
    dep.arrayOwnerDims = arrayOwnerDims;
    dep.accessSlots = *accessSlots;
    dep.haloShape = haloShape;
    dep.reduceScatter = *reduceScatter;
    depIndex[alloc.getOperation()].push_back(deps.size());
    deps.push_back(std::move(dep));
  };

  auto it = depIndex.find(alloc.getOperation());
  if (it == depIndex.end()) {
    appendDep();
    return success();
  }

  for (unsigned depIdx : it->second) {
    if (depIdx >= deps.size())
      continue;
    DirectDepSpec &dep = deps[depIdx];
    if (!canMergeAccessWindowIntoDep(dep, window.getMode(), haloShape))
      continue;
    if (dep.reduceScatter && window.getMode() != ArtsMode::in)
      continue;
    if (reduceScatter->has_value() && dep.mode != ArtsMode::in)
      continue;

    if (dep.ownerDimCount != ownerDimCount || dep.blockLo != facts->blockLo ||
        dep.blockHi != facts->blockHi ||
        dep.validExtents != facts->validExtents)
      return window.emitOpError()
             << "commits access-window evidence that conflicts with another "
                "window for the same DB";
    if (dep.arrayOwnerDims != arrayOwnerDims)
      return window.emitOpError()
             << "commits array owner-dim facts that conflict with another "
                "window for the same DB";
    if (dep.accessSlots != *accessSlots)
      return window.emitOpError()
             << "commits access-window block coordinates that conflict with "
                "another window for the same DB";
    dep.mode = arts::combineAccessModes(dep.mode, window.getMode());
    if (window.getMode() == ArtsMode::in && haloShape) {
      if (dep.haloShape && dep.haloShape != haloShape)
        return window.emitOpError()
               << "commits a halo shape that conflicts with another read "
                  "window for the same DB";
      dep.haloShape = haloShape;
    }
    if (reduceScatter->has_value()) {
      if (dep.reduceScatter &&
          (dep.reduceScatter->arrayId != (*reduceScatter)->arrayId ||
           dep.reduceScatter->sourceOwnerDims !=
               (*reduceScatter)->sourceOwnerDims ||
           dep.reduceScatter->sourceBlockShape !=
               (*reduceScatter)->sourceBlockShape))
        return window.emitOpError()
               << "commits reduce_scatter_like movement that conflicts with "
                  "another window for the same DB";
      dep.reduceScatter = **reduceScatter;
    }
    return success();
  }

  appendDep();
  return success();
}

LogicalResult
recordCuAccessWindowDependency(arts::DbAccessWindowOp window,
                               DenseMap<Operation *, unsigned> &depIndex,
                               SmallVectorImpl<DirectCuDepSpec> &deps) {
  auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
      arts::DbUtils::getUnderlyingDbAlloc(window.getMu()));
  if (!alloc)
    return window.emitOpError()
           << "does not reference an ARTS DB-backed MU after storage "
              "realization";

  unsigned ownerDimCount = static_cast<unsigned>(window.getOwnerDimCount());
  FailureOr<AccessWindowFacts> facts =
      getAccessWindowFacts(window, ownerDimCount);
  if (failed(facts))
    return failure();
  if (window.getHaloShapeAttr() && window.getMode() != ArtsMode::in)
    return window.emitOpError()
           << "commits a writable halo dependency; SDE must split the read "
              "halo and write access before ARTS realization";

  auto [it, inserted] = depIndex.try_emplace(alloc.getOperation(), deps.size());
  if (inserted) {
    ArrayAttr haloShape;
    if (window.getMode() == ArtsMode::in)
      haloShape = window.getHaloShapeAttr();
    deps.push_back(
        {alloc, window.getMode(), ownerDimCount,
         SmallVector<int64_t, 4>(facts->blockLo.begin(), facts->blockLo.end()),
         SmallVector<int64_t, 4>(facts->blockHi.begin(), facts->blockHi.end()),
         SmallVector<int64_t, 4>(facts->validExtents.begin(),
                                 facts->validExtents.end()),
         haloShape, Value{}});
    return success();
  }

  DirectCuDepSpec &dep = deps[it->second];
  if (dep.ownerDimCount != ownerDimCount || dep.blockLo != facts->blockLo ||
      dep.blockHi != facts->blockHi || dep.validExtents != facts->validExtents)
    return window.emitOpError()
           << "commits access-window evidence that conflicts with another "
              "window for the same DB in one CU";
  dep.mode = arts::combineAccessModes(dep.mode, window.getMode());
  if (window.getMode() == ArtsMode::in) {
    if (ArrayAttr haloShape = window.getHaloShapeAttr()) {
      if (dep.haloShape && dep.haloShape != haloShape)
        return window.emitOpError()
               << "commits a halo shape that conflicts with another read "
                  "window for the same DB in one CU";
      dep.haloShape = haloShape;
    }
  }
  return success();
}

LogicalResult
collectSuDependencies(sde::SdeSuIterateOp source,
                      SmallVectorImpl<DirectDepSpec> &deps,
                      DenseSet<Operation *> &consumedCuLevelAccessWindows,
                      SmallVectorImpl<Operation *> &consumedRedists) {
  DenseMap<Operation *, SmallVector<unsigned, 2>> depIndex;
  DenseSet<Operation *> touchedAllocs;
  collectTouchedDbAllocs(source, touchedAllocs);
  DenseMap<Operation *, SmallVector<ReduceScatterRedistFacts, 2>>
      reduceScatterFactsByAlloc;
  if (failed(collectPrecedingReduceScatterRedists(
          source, reduceScatterFactsByAlloc, consumedRedists)))
    return failure();

  auto consider = [&](arts::DbAccessWindowOp window) -> WalkResult {
    if (auto ownerSu = window->getParentOfType<sde::SdeSuIterateOp>())
      if (ownerSu != source)
        return WalkResult::advance();
    Operation *alloc = arts::DbUtils::getUnderlyingDbAlloc(window.getMu());
    if (!alloc || !touchedAllocs.contains(alloc))
      return WalkResult::advance();
    if (failed(recordAccessWindowDependency(source, window, depIndex, deps,
                                            reduceScatterFactsByAlloc)))
      return WalkResult::interrupt();
    if (!window->getParentOfType<sde::SdeSuIterateOp>())
      consumedCuLevelAccessWindows.insert(window.getOperation());
    return WalkResult::advance();
  };

  sde::SdeCuRegionOp parentCu = source->getParentOfType<sde::SdeCuRegionOp>();
  WalkResult result = parentCu ? parentCu.getBody().walk(consider)
                               : source.getBody().walk(consider);
  if (result.wasInterrupted())
    return failure();
  if (failed(verifyRawSuAccessesCoveredByDeps(source, depIndex, deps)))
    return failure();

  return success();
}

LogicalResult collectStandaloneCuDependencies(
    sde::SdeCuRegionOp source, SmallVectorImpl<DirectCuDepSpec> &deps,
    SmallVectorImpl<arts::DbAccessWindowOp> &windows) {
  DenseMap<Operation *, unsigned> depIndex;
  WalkResult result = source.getBody().walk([&](arts::DbAccessWindowOp window) {
    windows.push_back(window);
    if (failed(recordCuAccessWindowDependency(window, depIndex, deps)))
      return WalkResult::interrupt();
    return WalkResult::advance();
  });
  return result.wasInterrupted() ? failure() : success();
}

} // namespace mlir::carts::arts::boundary
