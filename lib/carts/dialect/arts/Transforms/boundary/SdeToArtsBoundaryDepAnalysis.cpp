///==========================================================================///
/// File: SdeToArtsBoundaryDepAnalysis.cpp
/// SDE access-window and dependency analysis for ARTS realization.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
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
  if (std::optional<sde::CommittedSuPhysicalLayout> layout =
          sde::recoverCommittedPhysicalLayout(source))
    return CommittedPhysicalLayout{layout->ownerDims, layout->blockShape};
  return std::nullopt;
}

std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromSuIterateOwnerFacts(sde::SdeSuIterateOp source) {
  return readPhysicalLayoutFromSuIterateAttrs(source);
}

std::optional<CommittedPhysicalLayout>
readPhysicalLayoutFromDepWindow(sde::SdeSuIterateOp source,
                                const DirectDepSpec &dep) {
  if (dep.ownerDimCount == 0 || dep.validExtents.empty())
    return std::nullopt;

  CommittedPhysicalLayout layout;
  layout.blockShape.assign(dep.validExtents.begin(), dep.validExtents.end());

  if (dep.arrayOwnerDims && !dep.arrayOwnerDims->empty()) {
    layout.ownerDims.assign(dep.arrayOwnerDims->begin(),
                            dep.arrayOwnerDims->end());
    return layout;
  }
  if (std::optional<SmallVector<int64_t, 4>> ownerDims =
          readI64ArrayAttr(source.getOwnerDimsAttr())) {
    layout.ownerDims.assign(ownerDims->begin(), ownerDims->end());
    return layout;
  }
  if (std::optional<sde::CommittedSuPhysicalLayout> committed =
          sde::recoverCommittedPhysicalLayout(source)) {
    layout.ownerDims.assign(committed->ownerDims.begin(),
                            committed->ownerDims.end());
    return layout;
  }
  return std::nullopt;
}

std::optional<CommittedPhysicalLayout>
readCommittedPhysicalLayout(sde::SdeSuIterateOp source,
                            ArrayRef<DirectDepSpec> deps) {
  if (std::optional<CommittedPhysicalLayout> fromAttrs =
          readPhysicalLayoutFromSuIterateOwnerFacts(source))
    return fromAttrs;
  for (const DirectDepSpec &dep : deps) {
    if (std::optional<CommittedPhysicalLayout> fromDep =
            readPhysicalLayoutFromDepWindow(source, dep))
      return fromDep;
  }
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
    return isStaticDbOwnerGroupedBlockScheduleRouteLocal(
        spec.dbSizes, counts, totalNodes, spec.ownerFacts);
  });
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
  return (lhs + rhs - 1) / rhs;
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
    if (physicalDim < 0 ||
        static_cast<size_t>(physicalDim) >= dep.validExtents.size())
      return source.emitOpError()
             << "dependency array owner dimension is outside the "
                "access-window payload rank";
    return static_cast<unsigned>(physicalDim);
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
    if (spec.dbSizes.size() != upperCounts.size())
      return std::nullopt;
    for (auto &&[upper, dbSize] : llvm::zip_equal(upperCounts, spec.dbSizes))
      upper = std::min(upper, dbSize);
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
    int64_t totalNodes, bool &splitToOwnerLocalGroups) {
  splitToOwnerLocalGroups = false;
  if (totalNodes <= 1 ||
      !llvm::any_of(groupBlockCounts, [](int64_t count) { return count > 1; }))
    return success();

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
    if (!dbSizes || dbSizes->size() != groupBlockCounts.size())
      return source.emitOpError()
             << "commits logicalWorkerSlice whose grouped distributed writer "
                "range cannot be proven owner-local from static DB block-grid "
                "facts";

    writerSpecs.push_back({*dbSizes, *ownerFacts});
  }

  std::optional<SmallVector<int64_t, 4>> ownerLocalCounts =
      findLargestOwnerLocalGroupCounts(writerSpecs, groupBlockCounts,
                                       totalNodes);
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
  if (mode == ArtsMode::out || mode == ArtsMode::inout)
    return role == sde::LayoutGraphRole::write;
  return false;
}

FailureOr<std::optional<SmallVector<int64_t, 4>>>
getArrayOwnerDimsForWindow(sde::SdeSuIterateOp source,
                           arts::DbAccessWindowOp window, ArtsMode mode,
                           const AccessWindowFacts &facts) {
  ArrayAttr layout = source.getArrayLayoutAttr();
  if (!layout)
    return std::optional<SmallVector<int64_t, 4>>{};
  IntegerAttr arrayId = window.getArrayIdAttr();
  if (!arrayId)
    return std::optional<SmallVector<int64_t, 4>>{};

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
    return std::optional<SmallVector<int64_t, 4>>{};
  if (match->ownerDims.size() != static_cast<size_t>(window.getOwnerDimCount()))
    return window.emitOpError()
           << "owner-dim count disagrees with committed SDE arrayLayout facts";

  for (int64_t ownerDim : match->ownerDims) {
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= facts.validExtents.size())
      return window.emitOpError()
             << "arrayLayout owner dimension is outside the access-window "
                "payload rank";
  }

  return std::optional<SmallVector<int64_t, 4>>{SmallVector<int64_t, 4>(
      match->ownerDims.begin(), match->ownerDims.end())};
}

void dropIdentityArrayOwnerDims(
    std::optional<SmallVector<int64_t, 4>> &arrayOwnerDims,
    unsigned ownerDimCount) {
  if (!arrayOwnerDims ||
      arrayOwnerDims->size() != static_cast<size_t>(ownerDimCount))
    return;
  for (auto [slot, ownerDim] : llvm::enumerate(*arrayOwnerDims))
    if (ownerDim != static_cast<int64_t>(slot))
      return;
  arrayOwnerDims.reset();
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
  for (int64_t ownerDim : *ownerDims) {
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= facts.validExtents.size())
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

std::optional<int64_t>
tryGetUnsignedUpperExclusive(Value value, ArrayRef<MappedLoopIv> mappedIvs,
                             unsigned depth = 0) {
  if (!value || depth > 8)
    return std::nullopt;
  value = ValueAnalysis::stripNumericCasts(value);
  if (std::optional<int64_t> cst = ValueAnalysis::tryFoldConstantIndex(value))
    return *cst >= 0 ? std::optional<int64_t>(*cst + 1) : std::nullopt;

  for (const MappedLoopIv &mapped : mappedIvs) {
    if (!ValueAnalysis::sameValue(value, mapped.iv))
      continue;
    if (mapped.lowerBound && mapped.upperBound && *mapped.lowerBound >= 0)
      return *mapped.upperBound;
    return std::nullopt;
  }

  if (auto rem = value.getDefiningOp<arith::RemUIOp>()) {
    std::optional<int64_t> divisor = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(rem.getRhs()));
    if (divisor && *divisor > 0)
      return *divisor;
    return std::nullopt;
  }
  if (auto div = value.getDefiningOp<arith::DivUIOp>()) {
    std::optional<int64_t> divisor = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(div.getRhs()));
    std::optional<int64_t> numeratorUpper =
        tryGetUnsignedUpperExclusive(div.getLhs(), mappedIvs, depth + 1);
    if (divisor && *divisor > 0 && numeratorUpper)
      return ceilDivPositiveI64(*numeratorUpper, *divisor);
    return std::nullopt;
  }
  if (auto add = value.getDefiningOp<arith::AddIOp>()) {
    std::optional<int64_t> lhs =
        tryGetUnsignedUpperExclusive(add.getLhs(), mappedIvs, depth + 1);
    std::optional<int64_t> rhs =
        tryGetUnsignedUpperExclusive(add.getRhs(), mappedIvs, depth + 1);
    if (lhs && rhs)
      return *lhs + *rhs - 1;
    return std::nullopt;
  }
  if (auto mul = value.getDefiningOp<arith::MulIOp>()) {
    std::optional<int64_t> lhs =
        tryGetUnsignedUpperExclusive(mul.getLhs(), mappedIvs, depth + 1);
    std::optional<int64_t> rhs =
        tryGetUnsignedUpperExclusive(mul.getRhs(), mappedIvs, depth + 1);
    if (lhs && rhs)
      return (*lhs - 1) * (*rhs - 1) + 1;
    return std::nullopt;
  }
  if (auto select = value.getDefiningOp<arith::SelectOp>()) {
    std::optional<int64_t> trueUpper = tryGetUnsignedUpperExclusive(
        select.getTrueValue(), mappedIvs, depth + 1);
    std::optional<int64_t> falseUpper = tryGetUnsignedUpperExclusive(
        select.getFalseValue(), mappedIvs, depth + 1);
    if (trueUpper && falseUpper)
      return std::max(*trueUpper, *falseUpper);
  }
  return std::nullopt;
}

bool isUnsignedLessThan(Value value, int64_t limit,
                        ArrayRef<MappedLoopIv> mappedIvs) {
  if (!value)
    return true;
  std::optional<int64_t> upper = tryGetUnsignedUpperExclusive(value, mappedIvs);
  return upper && *upper <= limit;
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

  if (expr == dimExpr)
    return DepOwnerAccessSlot{*loopDim, blockSize, std::nullopt};

  if (allowUnitHaloOffset) {
    if (auto add = dyn_cast<AffineBinaryOpExpr>(expr)) {
      if (add.getKind() == AffineExprKind::Add) {
        auto offset = dyn_cast<AffineConstantExpr>(add.getRHS());
        if (offset && offset.getValue() >= -1 && offset.getValue() <= 1 &&
            add.getLHS() == dimExpr)
          return DepOwnerAccessSlot{*loopDim, blockSize, std::nullopt};
        offset = dyn_cast<AffineConstantExpr>(add.getLHS());
        if (offset && offset.getValue() >= -1 && offset.getValue() <= 1 &&
            add.getRHS() == dimExpr)
          return DepOwnerAccessSlot{*loopDim, blockSize, std::nullopt};
      }
    }
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
        return DepOwnerAccessSlot{*loopDim, blockSize / multiplier,
                                  std::nullopt};
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

bool dependsOnDispatchLoop(Value value, ArrayRef<MappedLoopIv> mappedIvs) {
  if (!value)
    return false;
  for (const MappedLoopIv &mapped : mappedIvs) {
    if (!mapped.loopDim)
      continue;
    if (ValueAnalysis::sameValue(value, mapped.iv) ||
        ValueAnalysis::dependsOn(value, mapped.iv))
      return true;
  }
  return false;
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
    payloadDim = (*arrayOwnerDims)[slot];
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
        if (isCommittedFullWindowSlot(source, alloc, slot, blockLo, blockHi,
                                      validExtents, arrayOwnerDims)) {
          DepOwnerAccessSlot fullWindow;
          fullWindow.fullWindow = true;
          fullWindow.coordinateBlockSize = 1;
          candidate.push_back(fullWindow);
          continue;
        }
        if (mode == ArtsMode::in && slot < blockLo.size() &&
            slot < blockHi.size() && blockLo[slot] == 0 &&
            blockHi[slot] > blockLo[slot]) {
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
        if (isCommittedFullWindowSlot(source, alloc, slot, blockLo, blockHi,
                                      validExtents, arrayOwnerDims)) {
          DepOwnerAccessSlot fullWindow;
          fullWindow.fullWindow = true;
          fullWindow.coordinateBlockSize = 1;
          candidate.push_back(fullWindow);
          continue;
        }
        if (mode == ArtsMode::in && slot < blockLo.size() &&
            slot < blockHi.size() && blockLo[slot] == 0 &&
            blockHi[slot] > blockLo[slot] &&
            !dependsOnDispatchLoop(indices[slot], mappedIvs) &&
            isUnsignedLessThan(indices[slot], blockHi[slot], mappedIvs)) {
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
    if (selected.empty()) {
      selected = std::move(candidate);
      return success();
    }
    if (selected != candidate)
      return op->emitError()
             << "uses inconsistent block coordinates for one SDE access window";
    return success();
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
      if (selected.empty())
        selected = *candidate;
      else if (selected != *candidate)
        return op->emitError()
                   << "uses inconsistent block coordinates for one SDE "
                      "access window",
               WalkResult::interrupt();
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
      if (selected.empty())
        selected = *candidate;
      else if (selected != *candidate)
        return op->emitError()
                   << "uses inconsistent block coordinates for one SDE "
                      "access window",
               WalkResult::interrupt();
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
    return window.emitOpError()
           << "matches reduce_scatter_like movement but is not read-only";

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

LogicalResult verifyRawSuAccessCoveredByDep(
    sde::SdeSuIterateOp source, Operation *site, Value memref, ArtsMode mode,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps) {
  arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
  if (!alloc) {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && isDefinedInside(root, source.getOperation()))
      return success();
    if (isStackScratchMemref(memref))
      return success();
    return site->emitError()
           << "accesses external memref without ARTS DB-backed storage during "
              "direct SDE-to-ARTS SU realization";
  }

  auto it = depIndex.find(alloc.getOperation());
  if (it != depIndex.end()) {
    for (unsigned depIdx : it->second) {
      if (depIdx >= deps.size())
        continue;
      std::optional<unsigned> match =
          findDirectDepIndexForAccess(deps, alloc, mode);
      if (match && *match == depIdx)
        return success();
    }
    return site->emitError()
           << "raw access strengthens a committed SDE access-window "
              "dependency; SDE must author the dependency mode before "
              "direct ARTS lowering";
  }

  return site->emitError()
         << "touches a DB without a committed SDE access-window dependency; "
            "SDE must author the dependency before direct ARTS lowering";
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

LogicalResult verifyRawSuAccessesCoveredByDeps(
    sde::SdeSuIterateOp source,
    DenseMap<Operation *, SmallVector<unsigned, 2>> &depIndex,
    SmallVectorImpl<DirectDepSpec> &deps) {
  if (deps.empty())
    return success();
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  WalkResult result = computeBlock->walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(source, op, load.getMemref(),
                                                  ArtsMode::in, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(
                 source, op, store.getMemref(), ArtsMode::out, depIndex, deps))
                 ? WalkResult::interrupt()
                 : WalkResult::advance();
    if (auto copy = dyn_cast<memref::CopyOp>(op)) {
      if (failed(verifyRawSuAccessCoveredByDep(source, op, copy.getSource(),
                                               ArtsMode::in, depIndex, deps)))
        return WalkResult::interrupt();
      if (failed(verifyRawSuAccessCoveredByDep(source, op, copy.getTarget(),
                                               ArtsMode::out, depIndex, deps)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    }
    if (auto atomic = dyn_cast<sde::SdeCuAtomicOp>(op))
      return failed(verifyRawSuAccessCoveredByDep(
                 source, op, atomic.getAddr(), ArtsMode::inout, depIndex, deps))
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
  dropIdentityArrayOwnerDims(arrayOwnerDims, ownerDimCount);
  FailureOr<SmallVector<DepOwnerAccessSlot, 4>> accessSlots =
      deriveDepOwnerAccessSlots(source, alloc, window.getMode(), ownerDimCount,
                                facts->blockLo, facts->blockHi,
                                facts->validExtents, arrayOwnerDims,
                                window.getHaloShapeAttr() != nullptr);
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
  return verifyRawSuAccessesCoveredByDeps(source, depIndex, deps);
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
