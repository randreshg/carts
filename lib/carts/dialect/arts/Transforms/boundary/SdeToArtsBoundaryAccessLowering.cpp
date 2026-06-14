///==========================================================================///
/// File: SdeToArtsBoundaryAccessLowering.cpp
/// SDE access-window analysis, halo realization, and carrier lowering.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCuTask.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryDepAnalysis.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryHelpers.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryStandaloneCu.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundarySuIterate.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbLayoutFactsUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AffineMap.h"
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
                            ArtsMode mode, bool preferHaloRead = false) {
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
                                      ArrayRef<int64_t> counts,
                                      int64_t totalNodes) {
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

FailureOr<unsigned>
getAccessWindowPayloadDim(sde::SdeSuIterateOp source, const DirectDepSpec &dep,
                          unsigned ownerSlot, unsigned dispatchPhysicalDim) {
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

FailureOr<int64_t>
getAccessWindowPayloadExtent(sde::SdeSuIterateOp source,
                             const DirectDepSpec &dep, unsigned depPayloadDim) {
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


FailureOr<AccessWindowFacts>
getAccessWindowFacts(arts::DbAccessWindowOp window, unsigned ownerDimCount) {
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

SmallVector<MappedLoopIv, 8>
collectMappedLoopIvs(sde::SdeSuIterateOp source, Block *computeBlock) {
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

static std::optional<unsigned>
loopDimForIv(Value iv, ArrayRef<MappedLoopIv> mappedIvs) {
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

static std::optional<DepOwnerAccessSlot> analyzeDepOwnerAccessSlotFromAffineExpr(
    AffineExpr expr, ArrayRef<MappedLoopIv> mappedIvs,
    ArrayRef<Value> ivOrder, bool allowUnitHaloOffset) {
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
        return DepOwnerAccessSlot{*loopDim, blockSize / multiplier, std::nullopt};
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

bool dependsOnDispatchLoop(Value value,
                                  ArrayRef<MappedLoopIv> mappedIvs) {
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
  auto analyzeMapResults =
      [&](Operation *op, AffineMap map,
          ValueRange mapOperands) -> FailureOr<SmallVector<DepOwnerAccessSlot, 4>> {
    if (!map || map.getNumSymbols() != 0)
      return op->emitError()
             << "cannot analyze symbolic affine access map for SDE access-window "
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

LogicalResult
recordCoarseSuAccess(sde::SdeSuIterateOp source, Operation *site, Value memref,
                     ArtsMode mode, DenseMap<Operation *, unsigned> &depIndex,
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

LogicalResult collectExternalScalarCaptures(sde::SdeSuIterateOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  for (Value value : source.getLowerBounds())
    addIfExternalScalar(value);
  for (Value value : source.getUpperBounds())
    addIfExternalScalar(value);
  for (Value value : source.getSteps())
    addIfExternalScalar(value);

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  computeBlock->walk([&](Operation *op) {
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

LogicalResult collectExternalScalarCaptures(sde::SdeCuTaskOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  source.getBody().walk([&](Operation *op) {
    if (isa<sde::SdeMuDepOp>(op))
      return;
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

LogicalResult collectExternalScalarCaptures(sde::SdeCuRegionOp source,
                                                   SetVector<Value> &captures) {
  auto addIfExternalScalar = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value))
      return;
    if (!isDefinedInside(value, source.getOperation()))
      captures.insert(value);
  };

  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessWindowOp, sde::SdeMuDepOp>(op))
      return;
    for (Value operand : op->getOperands())
      addIfExternalScalar(operand);
  });
  return success();
}

Value remapOrSelf(IRMapping &mapper, Value value) {
  if (Value mapped = mapper.lookupOrNull(value))
    return mapped;
  return value;
}

bool enqueueForwardedMemrefResults(Operation *user, Value value,
                                          SmallVectorImpl<Value> &worklist) {
  if (auto cast = dyn_cast<memref::CastOp>(user)) {
    if (cast.getSource() != value)
      return false;
    worklist.push_back(cast.getResult());
    return true;
  }
  if (auto subview = dyn_cast<memref::SubViewOp>(user)) {
    if (subview.getSource() != value)
      return false;
    worklist.push_back(subview.getResult());
    return true;
  }
  if (auto unrealized = dyn_cast<UnrealizedConversionCastOp>(user)) {
    if (!llvm::is_contained(unrealized.getInputs(), value))
      return false;
    for (Value result : unrealized.getOutputs())
      if (isa<MemRefType>(result.getType()))
        worklist.push_back(result);
    return true;
  }
  return false;
}

bool isReadOnlyMemrefUseInside(Value source, Operation *scope) {
  SmallVector<Value, 8> worklist;
  DenseSet<Value> visited;
  worklist.push_back(source);

  while (!worklist.empty()) {
    Value current = worklist.pop_back_val();
    if (!current || !visited.insert(current).second)
      continue;

    for (Operation *user : current.getUsers()) {
      if (!scope->isAncestor(user))
        continue;
      if (auto access = arts::DbUtils::getMemoryAccessInfo(user)) {
        if (access->memref == current &&
            access->kind == arts::DbUtils::MemoryAccessKind::Write)
          return false;
        continue;
      }
      if (auto dim = dyn_cast<memref::DimOp>(user)) {
        if (dim.getSource() == current)
          continue;
      }
      if (enqueueForwardedMemrefResults(user, current, worklist))
        continue;
      return false;
    }
  }

  return true;
}

FailureOr<SmallVector<int64_t, 4>>
getOwnerHaloRadii(ArrayAttr haloShape, unsigned ownerDimCount,
                  Operation *context) {
  SmallVector<int64_t, 4> radii(ownerDimCount, 0);
  if (!haloShape)
    return radii;

  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(haloShape);
  if (!parsed) {
    context->emitError()
        << "has non-integer halo shape on a committed read dependency";
    return failure();
  }
  if (parsed->size() < ownerDimCount) {
    context->emitError()
        << "has halo shape with fewer entries than owner dimensions";
    return failure();
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    int64_t radius = (*parsed)[slot];
    if (radius < 0) {
      context->emitError()
          << "has negative halo radius on a committed read dependency";
      return failure();
    }
    radii[slot] = radius;
  }
  return radii;
}

bool hasGroupedOwnerBlocks(ArrayRef<int64_t> groupBlockCounts) {
  return llvm::any_of(groupBlockCounts,
                      [](int64_t count) { return count > 1; });
}

FailureOr<int64_t>
requireStaticPositiveIndex(Value value, Operation *context, StringRef name) {
  std::optional<int64_t> folded = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(value));
  if (!folded || *folded <= 0) {
    context->emitError() << "requires static positive " << name
                         << " for ARTS compact halo face realization";
    return failure();
  }
  return *folded;
}

void attachStencilHaloAcquireFacts(sde::SdeSuIterateOp source,
                                          arts::DbAcquireOp acquire,
                                          ArrayRef<int64_t> minOffsets,
                                          ArrayRef<int64_t> maxOffsets) {
  acquire.setDepPatternAttr(
      ArtsDepPatternAttr::get(source.getContext(), ArtsDepPattern::stencil));
  acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
      source.getContext(), EdtDistributionPattern::stencil));
  OpBuilder attrBuilder(source.getContext());
  acquire->setAttr(acquire.getStencilMinOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(minOffsets));
  acquire->setAttr(acquire.getStencilMaxOffsetsAttrName(),
                   attrBuilder.getI64ArrayAttr(maxOffsets));
  if (auto ownerDims = source.getOwnerDimsAttr())
    acquire->setAttr(acquire.getStencilOwnerDimsAttrName(), ownerDims);
  if (auto spatialDims = source.getSpatialDimsAttr())
    acquire->setAttr(acquire.getStencilSpatialDimsAttrName(), spatialDims);
  acquire->setAttr(acquire.getStencilSupportedBlockHaloAttrName(),
                   UnitAttr::get(source.getContext()));
}

FailureOr<CompactHaloColumnSpec>
realizeCompactHaloColumnPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                              ArrayRef<int64_t> groupBlockCounts,
                              OpBuilder &builder, Location loc) {
  if (dep.ownerDimCount != 2) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank is not supported by "
              "ARTS compact 2D unit-halo face realization; refusing a "
              "full-block halo byte-window";
  }
  if (hasGroupedOwnerBlocks(groupBlockCounts)) {
    return source.emitOpError()
           << "commits grouped halo CUs, but ARTS compact halo face "
              "realization currently requires one CU per DB block; "
              "refusing a full-block halo byte-window";
  }

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (haloRadii->size() != 2 || (*haloRadii)[0] != 1 || (*haloRadii)[1] != 1) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported 2D unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  if (dep.alloc.getSizes().size() != 2 ||
      dep.alloc.getElementSizes().size() != 4) {
    return source.emitOpError()
           << "commits a rank shape that ARTS compact 2D unit-halo face "
              "realization cannot represent; refusing a full-block halo "
              "byte-window";
  }

  FailureOr<int64_t> rowExtentStatic = requireStaticPositiveIndex(
      dep.alloc.getElementSizes()[2], source.getOperation(), "halo row extent");
  FailureOr<int64_t> colExtentStatic =
      requireStaticPositiveIndex(dep.alloc.getElementSizes()[3],
                                 source.getOperation(), "halo column extent");
  if (failed(rowExtentStatic) || failed(colExtentStatic))
    return failure();

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);
  Value rowExtent = dep.alloc.getElementSizes()[2];
  Value colExtent = dep.alloc.getElementSizes()[3];
  SmallVector<Value, 4> compactElementSizes{one, one, rowExtent,
                                            createOneIndex(builder, loc)};
  SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
  compactPhysicalBlockShape[3] = 1;

  auto createCompactDb = [&]() -> FailureOr<arts::DbAllocOp> {
    auto db = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    db.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDistributionAttrs(dep.alloc.getOperation(), db.getOperation());
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, db))
      return db.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    return db;
  };

  CompactHaloColumnSpec spec;
  FailureOr<arts::DbAllocOp> leftColumnDb = createCompactDb();
  if (failed(leftColumnDb))
    return failure();
  FailureOr<arts::DbAllocOp> rightColumnDb = createCompactDb();
  if (failed(rightColumnDb))
    return failure();
  spec.leftColumnDb = *leftColumnDb;
  spec.rightColumnDb = *rightColumnDb;
  spec.rowExtent = rowExtent;
  spec.colExtent = colExtent;

  auto outer = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[0],
                                  createOneIndex(builder, loc));
  builder.setInsertionPointToStart(outer.getBody());
  auto inner = scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[1],
                                  createOneIndex(builder, loc));
  builder.setInsertionPointToStart(inner.getBody());

  Value blockI = outer.getInductionVar();
  Value blockJ = inner.getInductionVar();
  SmallVector<Value> blockOffsets{blockI, blockJ};
  SmallVector<Value> blockSizes{createOneIndex(builder, loc),
                                createOneIndex(builder, loc)};

  auto sourceAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  sourceAcquire.setPreserveAccessMode();
  auto leftAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.leftColumnDb.getGuid(),
      spec.leftColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  leftAcquire.setPreserveAccessMode();
  auto rightAcquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::out, spec.rightColumnDb.getGuid(),
      spec.rightColumnDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, blockOffsets, blockSizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, Value{}, SmallVector<Value>{},
      SmallVector<Value>{});
  rightAcquire.setPreserveAccessMode();

  SmallVector<Value, 4> packDeps{sourceAcquire.getPtr(), leftAcquire.getPtr(),
                                 rightAcquire.getPtr()};
  SmallVector<Value, 4> packParams{rowExtent, colExtent};
  auto packEdt = arts::EdtOp::create(
      builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
      arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
  packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

  Block &packBlock = packEdt.getBody().front();
  for (Value depValue : packDeps)
    packBlock.addArgument(depValue.getType(), loc);
  unsigned paramOffset = packBlock.getNumArguments();
  for (Value param : packParams)
    packBlock.addArgument(param.getType(), loc);

  OpBuilder bodyBuilder(packEdt.getContext());
  bodyBuilder.setInsertionPointToStart(&packBlock);
  Value sourcePayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(0));
  Value leftPayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(1));
  Value rightPayload =
      arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(2));
  Value rowLimit = packBlock.getArgument(paramOffset);
  Value colLimit = packBlock.getArgument(paramOffset + 1);
  Value lastCol = arith::SubIOp::create(bodyBuilder, loc, colLimit,
                                        createOneIndex(bodyBuilder, loc));
  auto rowLoop =
      scf::ForOp::create(bodyBuilder, loc, createZeroIndex(bodyBuilder, loc),
                         rowLimit, createOneIndex(bodyBuilder, loc));
  bodyBuilder.setInsertionPointToStart(rowLoop.getBody());
  Value row = rowLoop.getInductionVar();
  SmallVector<Value, 4> sourceLeftIdx{createZeroIndex(bodyBuilder, loc),
                                      createZeroIndex(bodyBuilder, loc), row,
                                      createZeroIndex(bodyBuilder, loc)};
  Value leftValue =
      memref::LoadOp::create(bodyBuilder, loc, sourcePayload, sourceLeftIdx);
  memref::StoreOp::create(bodyBuilder, loc, leftValue, leftPayload,
                          sourceLeftIdx);
  SmallVector<Value, 4> sourceRightIdx{createZeroIndex(bodyBuilder, loc),
                                       createZeroIndex(bodyBuilder, loc), row,
                                       lastCol};
  SmallVector<Value, 4> compactRightIdx{createZeroIndex(bodyBuilder, loc),
                                        createZeroIndex(bodyBuilder, loc), row,
                                        createZeroIndex(bodyBuilder, loc)};
  Value rightValue =
      memref::LoadOp::create(bodyBuilder, loc, sourcePayload, sourceRightIdx);
  memref::StoreOp::create(bodyBuilder, loc, rightValue, rightPayload,
                          compactRightIdx);
  bodyBuilder.setInsertionPointToEnd(&packBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(outer);
  return spec;
}

arts::DbAcquireOp
create2DUnitRowHaloAcquire(sde::SdeSuIterateOp source, DirectDepSpec dep,
                           ArrayRef<Value> currentBlockOffsets,
                           CompactHaloColumnSpec spec, bool topFace,
                           SmallVectorImpl<Value> &dbOffsets,
                           OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = currentBlockOffsets[0];
  Value blockJ = currentBlockOffsets[1];
  Value sourceI;
  Value boundsValid;
  Value elementRowOffset;
  if (topFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockI, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockI, one);
    sourceI = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    elementRowOffset = arith::SubIOp::create(builder, loc, spec.rowExtent, one);
  } else {
    sourceI = arith::AddIOp::create(builder, loc, blockI, one);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceI, dep.alloc.getSizes()[0]);
    sourceI =
        arith::SelectOp::create(builder, loc, boundsValid, sourceI, blockI);
    elementRowOffset = zero;
  }

  SmallVector<Value> offsets{sourceI, blockJ};
  SmallVector<Value> sizes{one, createOneIndex(builder, loc)};
  dbOffsets.assign(offsets.begin(), offsets.end());
  SmallVector<Value> elementOffsets{
      createZeroIndex(builder, loc), createZeroIndex(builder, loc),
      elementRowOffset, createZeroIndex(builder, loc)};
  SmallVector<Value> elementSizes{createOneIndex(builder, loc),
                                  createOneIndex(builder, loc),
                                  createOneIndex(builder, loc), spec.colExtent};
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid, elementOffsets,
      elementSizes);
  acquire.setPreserveAccessMode();
  if (topFace)
    attachStencilHaloAcquireFacts(source, acquire, {-1, 0}, {0, 0});
  else
    attachStencilHaloAcquireFacts(source, acquire, {0, 0}, {1, 0});
  return acquire;
}

arts::DbAcquireOp create2DUnitCompactColumnAcquire(
    DirectDepSpec dep, ArrayRef<Value> currentBlockOffsets,
    CompactHaloColumnSpec spec, bool leftFace,
    SmallVectorImpl<Value> &dbOffsets, OpBuilder &builder, Location loc) {
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value blockI = currentBlockOffsets[0];
  Value blockJ = currentBlockOffsets[1];
  Value sourceJ;
  Value boundsValid;
  arts::DbAllocOp compactDb;
  if (leftFace) {
    Value canShift = arith::CmpIOp::create(
        builder, loc, arith::CmpIPredicate::uge, blockJ, one);
    Value shifted = arith::SubIOp::create(builder, loc, blockJ, one);
    sourceJ = arith::SelectOp::create(builder, loc, canShift, shifted, zero);
    boundsValid = canShift;
    compactDb = spec.rightColumnDb;
  } else {
    sourceJ = arith::AddIOp::create(builder, loc, blockJ, one);
    boundsValid = arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                        sourceJ, dep.alloc.getSizes()[1]);
    sourceJ =
        arith::SelectOp::create(builder, loc, boundsValid, sourceJ, blockJ);
    compactDb = spec.leftColumnDb;
  }

  SmallVector<Value> offsets{blockI, sourceJ};
  SmallVector<Value> sizes{one, createOneIndex(builder, loc)};
  dbOffsets.assign(offsets.begin(), offsets.end());
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, compactDb.getGuid(), compactDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

void enumerateUnitHaloSourceOffsets(
    unsigned rank, SmallVectorImpl<SmallVector<int64_t, 4>> &offsets) {
  SmallVector<int64_t, 4> current(rank, 0);
  std::function<void(unsigned, bool)> visit = [&](unsigned dim, bool nonzero) {
    if (dim == rank) {
      if (nonzero)
        offsets.push_back(current);
      return;
    }
    for (int64_t value : {-1, 0, 1}) {
      current[dim] = value;
      visit(dim + 1, nonzero || value != 0);
    }
  };
  visit(/*dim=*/0, /*nonzero=*/false);
}

SmallVector<Value, 4>
buildRankExpandedElementIndices(OpBuilder &builder, Location loc,
                                ArrayRef<Value> elementIndices) {
  SmallVector<Value, 4> indices;
  indices.reserve(elementIndices.size() * 2);
  for (unsigned idx = 0; idx < elementIndices.size(); ++idx)
    indices.push_back(createZeroIndex(builder, loc));
  indices.append(elementIndices.begin(), elementIndices.end());
  return indices;
}

void emitCompactHaloCopy(OpBuilder &builder, Location loc,
                                ArrayRef<int64_t> sourceOffsets,
                                ArrayRef<Value> elementExtents,
                                Value sourcePayload, Value compactPayload) {
  unsigned rank = sourceOffsets.size();
  SmallVector<Value, 4> loopIvs(rank);

  std::function<void(unsigned)> emitAtDim = [&](unsigned dim) {
    if (dim == rank) {
      SmallVector<Value, 4> sourceElementIndices;
      SmallVector<Value, 4> compactElementIndices;
      sourceElementIndices.reserve(rank);
      compactElementIndices.reserve(rank);
      for (unsigned slot = 0; slot < rank; ++slot) {
        if (sourceOffsets[slot] == 0) {
          sourceElementIndices.push_back(loopIvs[slot]);
          compactElementIndices.push_back(loopIvs[slot]);
          continue;
        }
        Value compactCoord = createZeroIndex(builder, loc);
        compactElementIndices.push_back(compactCoord);
        if (sourceOffsets[slot] > 0) {
          sourceElementIndices.push_back(createZeroIndex(builder, loc));
          continue;
        }
        Value last = arith::SubIOp::create(builder, loc, elementExtents[slot],
                                           createOneIndex(builder, loc));
        sourceElementIndices.push_back(last);
      }
      Value value = memref::LoadOp::create(
          builder, loc, sourcePayload,
          buildRankExpandedElementIndices(builder, loc, sourceElementIndices));
      memref::StoreOp::create(
          builder, loc, value, compactPayload,
          buildRankExpandedElementIndices(builder, loc, compactElementIndices));
      return;
    }

    if (sourceOffsets[dim] != 0) {
      emitAtDim(dim + 1);
      return;
    }

    auto loop =
        scf::ForOp::create(builder, loc, createZeroIndex(builder, loc),
                           elementExtents[dim], createOneIndex(builder, loc));
    builder.setInsertionPointToStart(loop.getBody());
    loopIvs[dim] = loop.getInductionVar();
    emitAtDim(dim + 1);
  };

  emitAtDim(/*dim=*/0);
}

FailureOr<CompactHaloNdSpec>
realizeCompactHaloNdPacks(sde::SdeSuIterateOp source, DirectDepSpec dep,
                          ArrayRef<int64_t> groupBlockCounts,
                          OpBuilder &builder, Location loc) {
  unsigned ownerDimCount = dep.ownerDimCount;
  if (ownerDimCount < 2) {
    return source.emitOpError()
           << "requires owner rank of at least 2 for compact N-D halo "
              "realization, got "
           << ownerDimCount;
  }
  if (ownerDimCount > 3) {
    return source.emitOpError()
           << "commits a halo dependency whose owner rank exceeds the "
              "implemented ARTS compact N-D unit-halo realization; refusing a "
              "full-block halo byte-window";
  }
  if (hasGroupedOwnerBlocks(groupBlockCounts)) {
    return source.emitOpError()
           << "commits grouped halo CUs, but ARTS compact N-D halo "
              "realization currently requires one CU per DB block; refusing a "
              "full-block halo byte-window";
  }

  FailureOr<SmallVector<int64_t, 4>> haloRadii = getOwnerHaloRadii(
      dep.haloShape, dep.ownerDimCount, source.getOperation());
  if (failed(haloRadii))
    return failure();
  if (llvm::any_of(*haloRadii, [](int64_t radius) { return radius != 1; })) {
    return source.emitOpError()
           << "commits a halo dependency outside the supported unit-halo "
              "shape; ARTS must realize the exact halo graph instead of "
              "widening to a full-block byte window";
  }

  if (dep.alloc.getSizes().size() != ownerDimCount ||
      dep.alloc.getElementSizes().size() != ownerDimCount * 2) {
    return source.emitOpError()
           << "commits a rank shape that ARTS compact N-D unit-halo "
              "realization cannot represent; refusing a full-block halo "
              "byte-window";
  }

  SmallVector<Value, 4> elementExtents;
  elementExtents.reserve(ownerDimCount);
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    FailureOr<int64_t> extent = requireStaticPositiveIndex(
        dep.alloc.getElementSizes()[ownerDimCount + slot],
        source.getOperation(), "halo element extent");
    if (failed(extent))
      return failure();
    elementExtents.push_back(dep.alloc.getElementSizes()[ownerDimCount + slot]);
  }

  SmallVector<int64_t, 4> physicalBlockShape;
  physicalBlockShape.reserve(dep.alloc.getElementSizes().size());
  for (Value size : dep.alloc.getElementSizes()) {
    FailureOr<int64_t> constant = requireStaticPositiveIndex(
        size, source.getOperation(), "compact halo block extent");
    if (failed(constant))
      return failure();
    physicalBlockShape.push_back(*constant);
  }

  SmallVector<SmallVector<int64_t, 4>, 8> sideOffsets;
  enumerateUnitHaloSourceOffsets(ownerDimCount, sideOffsets);

  MLIRContext *ctx = source.getContext();
  Value zero = createZeroIndex(builder, loc);
  Value one = createOneIndex(builder, loc);
  Value route = arts::createCurrentNodeRoute(builder, loc);

  CompactHaloNdSpec spec;
  spec.elementExtents.assign(elementExtents.begin(), elementExtents.end());
  spec.sides.reserve(sideOffsets.size());

  for (ArrayRef<int64_t> sideOffset : sideOffsets) {
    SmallVector<Value> compactElementSizes;
    compactElementSizes.reserve(ownerDimCount * 2);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      compactElementSizes.push_back(one);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      compactElementSizes.push_back(sideOffset[slot] == 0 ? elementExtents[slot]
                                                          : one);

    SmallVector<int64_t, 4> compactPhysicalBlockShape(physicalBlockShape);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      if (sideOffset[slot] != 0)
        compactPhysicalBlockShape[ownerDimCount + slot] = 1;

    auto payloadDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, route, DbAllocType::heap, DbMode::write,
        dep.alloc.getElementType(),
        SmallVector<Value>(dep.alloc.getSizes().begin(),
                           dep.alloc.getSizes().end()),
        SmallVector<Value>(compactElementSizes.begin(),
                           compactElementSizes.end()),
        PartitionMode::block);
    payloadDb.setCompactHaloPayloadAttr(UnitAttr::get(ctx));
    copyDistributionAttrs(dep.alloc.getOperation(), payloadDb.getOperation());
    if (hasDistributedDbAllocation(dep.alloc.getOperation()) &&
        !destDbOwnerRouteMatchesSourcePolicy(dep.alloc, payloadDb))
      return payloadDb.emitOpError()
             << "cannot derive compact halo owner routes from destination DB "
                "grid and source distribution kind";
    spec.sides.push_back(
        {SmallVector<int64_t, 4>(sideOffset.begin(), sideOffset.end()),
         payloadDb});
  }

  for (CompactHaloNdSideSpec &side : spec.sides) {
    SmallVector<Value> blockOffsets;
    blockOffsets.reserve(ownerDimCount);
    scf::ForOp outerLoop;
    for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
      auto loop =
          scf::ForOp::create(builder, loc, zero, dep.alloc.getSizes()[slot],
                             createOneIndex(builder, loc));
      if (!outerLoop)
        outerLoop = loop;
      blockOffsets.push_back(loop.getInductionVar());
      builder.setInsertionPointToStart(loop.getBody());
    }

    SmallVector<Value> blockSizes(ownerDimCount, one);
    auto sourceAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::in, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{},
        SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()),
        blockSizes, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    sourceAcquire.setPreserveAccessMode();
    auto payloadAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, side.payloadDb.getGuid(),
        side.payloadDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::block),
        SmallVector<Value>{},
        SmallVector<Value>(blockOffsets.begin(), blockOffsets.end()),
        blockSizes, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    payloadAcquire.setPreserveAccessMode();

    SmallVector<Value, 4> packDeps{sourceAcquire.getPtr(),
                                   payloadAcquire.getPtr()};
    SmallVector<Value, 4> packParams(elementExtents.begin(),
                                     elementExtents.end());
    auto packEdt = arts::EdtOp::create(
        builder, loc, arts::EdtType::task, arts::EdtConcurrency::intranode,
        arts::createCurrentNodeRoute(builder, loc), packDeps, packParams);
    packEdt.setCompactHaloPackAttr(UnitAttr::get(ctx));

    Block &packBlock = packEdt.getBody().front();
    for (Value depValue : packDeps)
      packBlock.addArgument(depValue.getType(), loc);
    unsigned paramOffset = packBlock.getNumArguments();
    for (Value param : packParams)
      packBlock.addArgument(param.getType(), loc);

    OpBuilder bodyBuilder(packEdt.getContext());
    bodyBuilder.setInsertionPointToStart(&packBlock);
    Value sourcePayload =
        arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(0));
    Value compactPayload =
        arts::realizeDbInnerPayload(bodyBuilder, loc, packBlock.getArgument(1));
    SmallVector<Value, 4> bodyElementExtents;
    bodyElementExtents.reserve(ownerDimCount);
    for (unsigned slot = 0; slot < ownerDimCount; ++slot)
      bodyElementExtents.push_back(packBlock.getArgument(paramOffset + slot));
    emitCompactHaloCopy(bodyBuilder, loc, side.sourceOffsets,
                        bodyElementExtents, sourcePayload, compactPayload);
    bodyBuilder.setInsertionPointToEnd(&packBlock);
    arts::YieldOp::create(bodyBuilder, loc);

    builder.setInsertionPointAfter(outerLoop);
  }

  return spec;
}

arts::DbAcquireOp createNdCompactHaloAcquire(
    DirectDepSpec dep, ArrayRef<Value> currentBlockOffsets,
    CompactHaloNdSideSpec side, SmallVectorImpl<Value> &dbOffsets,
    OpBuilder &builder, Location loc) {
  Value one = createOneIndex(builder, loc);
  Value boundsValid = {};
  auto appendBounds = [&](Value condition) {
    boundsValid = boundsValid ? arith::AndIOp::create(builder, loc, boundsValid,
                                                      condition)
                              : condition;
  };

  SmallVector<Value> offsets;
  offsets.reserve(currentBlockOffsets.size());
  for (auto [slot, sourceOffset] : llvm::enumerate(side.sourceOffsets)) {
    Value current = currentBlockOffsets[slot];
    if (sourceOffset < 0) {
      Value canShift = arith::CmpIOp::create(
          builder, loc, arith::CmpIPredicate::uge, current, one);
      Value shifted = arith::SubIOp::create(builder, loc, current, one);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      appendBounds(canShift);
      continue;
    }
    if (sourceOffset > 0) {
      Value shifted = arith::AddIOp::create(builder, loc, current, one);
      Value canShift =
          arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ult,
                                shifted, dep.alloc.getSizes()[slot]);
      offsets.push_back(
          arith::SelectOp::create(builder, loc, canShift, shifted, current));
      appendBounds(canShift);
      continue;
    }
    offsets.push_back(current);
  }

  if (!boundsValid)
    boundsValid = arith::ConstantIntOp::create(builder, loc, 1, 1);
  dbOffsets.assign(offsets.begin(), offsets.end());
  SmallVector<Value> sizes(currentBlockOffsets.size(), one);
  auto acquire = arts::DbAcquireOp::create(
      builder, loc, ArtsMode::in, side.payloadDb.getGuid(),
      side.payloadDb.getPtr(),
      std::optional<arts::PartitionMode>(arts::PartitionMode::block),
      SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
      SmallVector<Value>{}, SmallVector<Value>{}, boundsValid,
      SmallVector<Value>{}, SmallVector<Value>{});
  acquire.setPreserveAccessMode();
  return acquire;
}

Value getCommonDivRemSource(Value divValue, Value remValue,
                                   Value expectedDivisor) {
  auto div = ValueAnalysis::stripNumericCasts(divValue)
                 .getDefiningOp<arith::DivUIOp>();
  auto rem = ValueAnalysis::stripNumericCasts(remValue)
                 .getDefiningOp<arith::RemUIOp>();
  if (!div || !rem)
    return {};
  if (!ValueAnalysis::sameValue(div.getLhs(), rem.getLhs()) &&
      !ValueAnalysis::areValuesEquivalent(div.getLhs(), rem.getLhs()))
    return {};
  if ((!ValueAnalysis::sameValue(div.getRhs(), rem.getRhs()) &&
       !ValueAnalysis::areValuesEquivalent(div.getRhs(), rem.getRhs())) ||
      (!ValueAnalysis::sameValue(div.getRhs(), expectedDivisor) &&
       !ValueAnalysis::areValuesEquivalent(div.getRhs(), expectedDivisor)))
    return {};
  return div.getLhs();
}

FailureOr<std::optional<HaloLoadRewrite>>
classify2DUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const Halo2DTaskWork &work, Value rowIv, Value colIv) {
  OperandRange indices = load.getIndices();
  if (indices.size() != 4) {
    load.emitOpError()
        << "uses a rank shape unsupported by ARTS compact 2D unit-halo "
           "realization";
    return failure();
  }

  Value rowExpr = getCommonDivRemSource(indices[0], indices[2], work.rowExtent);
  Value colExpr = getCommonDivRemSource(indices[1], indices[3], work.colExtent);
  if (!rowExpr || !colExpr) {
    load.emitOpError()
        << "does not expose div/rem rank-expanded indices required for ARTS "
           "compact 2D unit-halo load rewriting";
    return failure();
  }

  std::optional<int64_t> rowOffset =
      sde::tryGetUnitNeighborhoodOffset(rowExpr, rowIv);
  std::optional<int64_t> colOffset =
      sde::tryGetUnitNeighborhoodOffset(colExpr, colIv);
  if (!rowOffset || !colOffset) {
    load.emitOpError()
        << "does not expose affine unit-neighborhood indices required for ARTS "
           "compact 2D unit-halo load rewriting";
    return failure();
  }
  if (*rowOffset == 0 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Center}};
  if (*rowOffset != 0 && *colOffset != 0) {
    load.emitOpError()
        << "requires corner halo realization, which ARTS has not "
           "committed; refusing a full-block halo byte-window";
    return failure();
  }
  if (*rowOffset == -1 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Top}};
  if (*rowOffset == 1 && *colOffset == 0)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Bottom}};
  if (*rowOffset == 0 && *colOffset == -1)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Left}};
  if (*rowOffset == 0 && *colOffset == 1)
    return std::optional<HaloLoadRewrite>{
        HaloLoadRewrite{haloWorkIndex, Halo2DFace::Right}};

  load.emitOpError()
      << "requires non-unit halo realization, which ARTS has not "
         "committed; refusing a full-block halo byte-window";
  return failure();
}

FailureOr<std::optional<HaloNdLoadRewrite>>
classifyNdUnitHaloLoad(memref::LoadOp load, unsigned haloWorkIndex,
                       const HaloNdTaskWork &work,
                       ArrayRef<Value> ownerLoopIvs) {
  unsigned rank = ownerLoopIvs.size();
  OperandRange indices = load.getIndices();
  if (indices.size() != rank * 2 || work.elementExtents.size() != rank) {
    load.emitOpError()
        << "uses a rank shape unsupported by ARTS compact N-D unit-halo "
           "realization";
    return failure();
  }

  bool hasHaloOffset = false;
  for (unsigned slot = 0; slot < rank; ++slot) {
    Value expr = getCommonDivRemSource(indices[slot], indices[rank + slot],
                                       work.elementExtents[slot]);
    if (!expr) {
      load.emitOpError()
          << "does not expose div/rem rank-expanded indices required for ARTS "
             "compact N-D unit-halo load rewriting";
      return failure();
    }
    std::optional<int64_t> offset =
        sde::tryGetUnitNeighborhoodOffset(expr, ownerLoopIvs[slot]);
    if (!offset) {
      load.emitOpError()
          << "does not expose affine unit-neighborhood indices required for "
             "ARTS compact N-D unit-halo load rewriting";
      return failure();
    }
    if (*offset < -1 || *offset > 1) {
      load.emitOpError()
          << "requires non-unit halo realization, which ARTS has not "
             "committed; refusing a full-block halo byte-window";
      return failure();
    }
    hasHaloOffset |= *offset != 0;
  }

  if (!hasHaloOffset)
    return std::optional<HaloNdLoadRewrite>{};
  return std::optional<HaloNdLoadRewrite>{HaloNdLoadRewrite{haloWorkIndex}};
}

FailureOr<bool> needsExactNdHaloFor2D(sde::SdeSuIterateOp source,
                                             DirectDepSpec dep,
                                             Block *computeBlock) {
  if (dep.ownerDimCount != 2)
    return false;
  if (dep.alloc.getElementSizes().size() != 4) {
    source.emitOpError() << "commits a rank shape that ARTS compact 2D "
                            "unit-halo realization cannot represent";
    return failure();
  }
  bool sawCorner = false;
  bool failedScan = false;
  WalkResult result = computeBlock->walk([&](memref::LoadOp load) {
    if (resolveBoundaryDbAlloc(load.getMemref()) != dep.alloc)
      return WalkResult::advance();
    OperandRange indices = load.getIndices();
    if (indices.size() != 4) {
      load.emitOpError()
          << "uses a rank shape unsupported by ARTS compact 2D unit-halo "
             "realization";
      failedScan = true;
      return WalkResult::interrupt();
    }
    SmallVector<Value, 4> loopIvs;
    for (Operation *parent = load->getParentOp(); parent;
         parent = parent->getParentOp())
      if (auto loop = dyn_cast<scf::ForOp>(parent))
        loopIvs.push_back(loop.getInductionVar());
    if (loopIvs.size() < 2) {
      load.emitOpError()
          << "is not nested in the 2D compute loops required for ARTS compact "
             "unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    Value rowIv = loopIvs[1];
    Value colIv = loopIvs[0];
    Value rowExpr = getCommonDivRemSource(indices[0], indices[2],
                                          dep.alloc.getElementSizes()[2]);
    Value colExpr = getCommonDivRemSource(indices[1], indices[3],
                                          dep.alloc.getElementSizes()[3]);
    if (!rowExpr || !colExpr) {
      load.emitOpError()
          << "does not expose div/rem rank-expanded indices required for ARTS "
             "compact 2D unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    std::optional<int64_t> rowOffset =
        sde::tryGetUnitNeighborhoodOffset(rowExpr, rowIv);
    std::optional<int64_t> colOffset =
        sde::tryGetUnitNeighborhoodOffset(colExpr, colIv);
    if (!rowOffset || !colOffset) {
      load.emitOpError()
          << "does not expose affine unit-neighborhood indices required for "
             "ARTS compact 2D unit-halo load rewriting";
      failedScan = true;
      return WalkResult::interrupt();
    }
    if (*rowOffset != 0 && *colOffset != 0)
      sawCorner = true;
    return WalkResult::advance();
  });
  if (result.wasInterrupted() || failedScan)
    return failure();
  return sawCorner;
}

SmallVector<Value, 4> buildRankExpandedElementIndices(OpBuilder &builder,
                                                             Location loc,
                                                             Value row,
                                                             Value col) {
  return SmallVector<Value, 4>{createZeroIndex(builder, loc),
                               createZeroIndex(builder, loc), row, col};
}

template <typename RewriteT>
LogicalResult recordClonedHaloLoadRewrites(
    Operation *original, Operation *cloned,
    const DenseMap<Operation *, RewriteT> &originalRewrites,
    DenseMap<Operation *, RewriteT> &clonedRewrites) {
  auto recordIfMapped = [&](Operation *originalLoad, Operation *clonedLoad) {
    auto it = originalRewrites.find(originalLoad);
    if (it != originalRewrites.end())
      clonedRewrites[clonedLoad] = it->second;
  };

  SmallVector<memref::LoadOp, 8> originalLoads;
  SmallVector<memref::LoadOp, 8> clonedLoads;
  if (auto load = dyn_cast<memref::LoadOp>(original))
    originalLoads.push_back(load);
  else
    original->walk([&](memref::LoadOp load) { originalLoads.push_back(load); });
  if (auto load = dyn_cast<memref::LoadOp>(cloned))
    clonedLoads.push_back(load);
  else
    cloned->walk([&](memref::LoadOp load) { clonedLoads.push_back(load); });

  bool hasMappedLoad = llvm::any_of(originalLoads, [&](memref::LoadOp load) {
    return originalRewrites.contains(load.getOperation());
  });
  if (!hasMappedLoad)
    return success();
  if (originalLoads.size() != clonedLoads.size())
    return cloned->emitError()
           << "could not preserve compact halo load rewrite mapping while "
              "cloning SDE compute body";
  for (auto [originalLoad, clonedLoad] : llvm::zip(originalLoads, clonedLoads))
    recordIfMapped(originalLoad.getOperation(), clonedLoad.getOperation());
  return success();
}

LogicalResult rewriteCloned2DUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloLoadRewrite> &rewrites,
    ArrayRef<Halo2DTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs) {
  OpBuilder builder(task.getContext());

  for (const auto &entry : rewrites) {
    auto load = dyn_cast_or_null<memref::LoadOp>(entry.first);
    if (!load)
      continue;
    const HaloLoadRewrite &rewrite = entry.second;
    if (rewrite.haloWorkIndex >= haloWorks.size())
      return task.emitOpError() << "has stale compact halo load rewrite state";
    const Halo2DTaskWork &work = haloWorks[rewrite.haloWorkIndex];
    auto requirePayload = [&](unsigned index) -> FailureOr<Value> {
      if (index >= payloads.size() || index >= depBlockOffsetArgs.size() ||
          depBlockOffsetArgs[index].size() != 2) {
        task.emitOpError() << "has inconsistent compact halo dependency state";
        return failure();
      }
      return payloads[index];
    };

    FailureOr<Value> centerPayload = requirePayload(work.centerTaskDepIndex);
    if (failed(centerPayload))
      return failure();
    OperandRange indices = load.getIndices();
    if (indices.size() != 4)
      return load.emitOpError()
             << "has unsupported compact halo rank after cloning";

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    Value elemRow = indices[2];
    Value elemCol = indices[3];
    if (rewrite.face == Halo2DFace::Center) {
      load.getIndicesMutable()[0].set(createZeroIndex(builder, loc));
      load.getIndicesMutable()[1].set(createZeroIndex(builder, loc));
      continue;
    }

    unsigned ownerSlot = 0;
    unsigned faceDepIndex = work.topTaskDepIndex;
    SmallVector<Value, 4> faceIndices;
    switch (rewrite.face) {
    case Halo2DFace::Top:
      ownerSlot = 0;
      faceDepIndex = work.topTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, createZeroIndex(builder, loc), elemCol);
      break;
    case Halo2DFace::Bottom:
      ownerSlot = 0;
      faceDepIndex = work.bottomTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, createZeroIndex(builder, loc), elemCol);
      break;
    case Halo2DFace::Left:
      ownerSlot = 1;
      faceDepIndex = work.leftTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, elemRow, createZeroIndex(builder, loc));
      break;
    case Halo2DFace::Right:
      ownerSlot = 1;
      faceDepIndex = work.rightTaskDepIndex;
      faceIndices = buildRankExpandedElementIndices(
          builder, loc, elemRow, createZeroIndex(builder, loc));
      break;
    case Halo2DFace::Center:
      llvm_unreachable("center handled above");
    }

    FailureOr<Value> facePayload = requirePayload(faceDepIndex);
    if (failed(facePayload))
      return failure();

    Value centerBlock = depBlockOffsetArgs[work.centerTaskDepIndex][ownerSlot];
    Value crossesBlock =
        arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ne,
                              indices[ownerSlot], centerBlock);
    SmallVector<Type, 1> resultTypes{load.getType()};
    auto ifOp = scf::IfOp::create(builder, loc, resultTypes, crossesBlock,
                                  /*withElseRegion=*/true);

    builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
    Value faceValue =
        memref::LoadOp::create(builder, loc, *facePayload, faceIndices);
    scf::YieldOp::create(builder, loc, ValueRange{faceValue});

    builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
    SmallVector<Value, 4> coreIndices =
        buildRankExpandedElementIndices(builder, loc, elemRow, elemCol);
    Value coreValue =
        memref::LoadOp::create(builder, loc, *centerPayload, coreIndices);
    scf::YieldOp::create(builder, loc, ValueRange{coreValue});

    load.replaceAllUsesWith(ifOp.getResult(0));
    load.erase();
  }

  return success();
}

LogicalResult rewriteClonedNdUnitHaloLoads(
    arts::EdtOp task, const DenseMap<Operation *, HaloNdLoadRewrite> &rewrites,
    ArrayRef<HaloNdTaskWork> haloWorks, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs) {
  OpBuilder builder(task.getContext());

  auto andValues = [&](Location loc, Value lhs, Value rhs) -> Value {
    return lhs ? arith::AndIOp::create(builder, loc, lhs, rhs).getResult()
               : rhs;
  };

  for (const auto &entry : rewrites) {
    auto load = dyn_cast_or_null<memref::LoadOp>(entry.first);
    if (!load)
      continue;
    const HaloNdLoadRewrite &rewrite = entry.second;
    if (rewrite.haloWorkIndex >= haloWorks.size())
      return task.emitOpError() << "has stale compact halo load rewrite state";
    const HaloNdTaskWork &work = haloWorks[rewrite.haloWorkIndex];
    unsigned rank = work.elementExtents.size();
    OperandRange indices = load.getIndices();
    if (indices.size() != rank * 2)
      return load.emitOpError() << "has unsupported compact halo rank after "
                                   "cloning";

    auto requirePayload = [&](unsigned index) -> FailureOr<Value> {
      if (index >= payloads.size() || index >= depBlockOffsetArgs.size() ||
          depBlockOffsetArgs[index].size() != rank) {
        task.emitOpError() << "has inconsistent compact halo dependency state";
        return failure();
      }
      return payloads[index];
    };

    FailureOr<Value> centerPayload = requirePayload(work.centerTaskDepIndex);
    if (failed(centerPayload))
      return failure();

    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    SmallVector<Value, 4> centerElementIndices;
    centerElementIndices.reserve(rank);
    for (unsigned slot = 0; slot < rank; ++slot)
      centerElementIndices.push_back(indices[rank + slot]);
    Value replacement = memref::LoadOp::create(
        builder, loc, *centerPayload,
        buildRankExpandedElementIndices(builder, loc, centerElementIndices));

    if (work.sideSourceOffsets.size() != work.sideTaskDepIndices.size())
      return task.emitOpError() << "has inconsistent compact halo side state";

    ArrayRef<Value> centerBlockOffsets =
        depBlockOffsetArgs[work.centerTaskDepIndex];
    for (auto [sideOffsets, sideDepIndex] :
         llvm::zip_equal(work.sideSourceOffsets, work.sideTaskDepIndices)) {
      FailureOr<Value> sidePayload = requirePayload(sideDepIndex);
      if (failed(sidePayload))
        return failure();
      ArrayRef<Value> sideBlockOffsets = depBlockOffsetArgs[sideDepIndex];

      Value condition;
      for (unsigned slot = 0; slot < rank; ++slot) {
        Value centerBlock = centerBlockOffsets[slot];
        if (sideOffsets[slot] == 0) {
          Value sameBlock =
              arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq,
                                    indices[slot], centerBlock);
          condition = andValues(loc, condition, sameBlock);
          continue;
        }

        Value crossesBlock = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ne, indices[slot], centerBlock);
        Value matchesSide =
            arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::eq,
                                  indices[slot], sideBlockOffsets[slot]);
        condition = andValues(loc, condition, crossesBlock);
        condition = andValues(loc, condition, matchesSide);
      }

      SmallVector<Value, 4> sideElementIndices;
      sideElementIndices.reserve(rank);
      for (unsigned slot = 0; slot < rank; ++slot)
        sideElementIndices.push_back(sideOffsets[slot] == 0
                                         ? indices[rank + slot]
                                         : createZeroIndex(builder, loc));

      auto ifOp = scf::IfOp::create(builder, loc, load.getType(), condition,
                                    /*withElseRegion=*/true);
      builder.setInsertionPointToStart(&ifOp.getThenRegion().front());
      Value sideValue = memref::LoadOp::create(
          builder, loc, *sidePayload,
          buildRankExpandedElementIndices(builder, loc, sideElementIndices));
      scf::YieldOp::create(builder, loc, ValueRange{sideValue});

      builder.setInsertionPointToStart(&ifOp.getElseRegion().front());
      scf::YieldOp::create(builder, loc, ValueRange{replacement});
      replacement = ifOp.getResult(0);
      builder.setInsertionPointAfter(ifOp);
    }

    load.replaceAllUsesWith(replacement);
    load.erase();
  }

  return success();
}

LogicalResult rewriteOwnerIndicesToLocal(
    arts::EdtOp task, ArrayRef<Value> payloads,
    ArrayRef<SmallVector<Value, 4>> depBlockOffsetArgs,
    ArrayRef<bool> depRequiresDbRef, ArrayRef<unsigned> depOwnerDimCounts,
    ArrayRef<SmallVector<int64_t, 4>> depGroupBlockCounts) {
  if (payloads.size() != depBlockOffsetArgs.size() ||
      payloads.size() != depRequiresDbRef.size() ||
      payloads.size() != depOwnerDimCounts.size() ||
      payloads.size() != depGroupBlockCounts.size())
    return task.emitOpError() << "has inconsistent owner grouping facts";
  for (auto [offsets, ownerDimCount, groupCounts] : llvm::zip_equal(
           depBlockOffsetArgs, depOwnerDimCounts, depGroupBlockCounts))
    if (offsets.size() != ownerDimCount || groupCounts.size() != ownerDimCount)
      return task.emitOpError() << "has inconsistent dependency block offsets";

  auto depHasGroupedBlocks = [&](unsigned depIdx) {
    return llvm::any_of(depGroupBlockCounts[depIdx],
                        [](int64_t count) { return count > 1; });
  };

  DenseMap<Value, unsigned> payloadToDepIndex;
  DenseMap<Value, Value> payloadSources;
  for (auto [idx, payload] : llvm::enumerate(payloads)) {
    auto [it, inserted] = payloadToDepIndex.try_emplace(payload, idx);
    if (!inserted)
      return task.emitOpError()
             << "has duplicate dependency payload during owner-index rewrite";
    if (!depHasGroupedBlocks(idx) && !depRequiresDbRef[idx])
      continue;
    auto ref = payload.getDefiningOp<arts::DbRefOp>();
    if (!ref)
      return task.emitOpError()
             << "cannot reindex owner blocks without a DB-ref payload";
    payloadSources[payload] = ref.getSource();
  }

  OpBuilder builder(task.getContext());
  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    auto depIt = payloadToDepIndex.find(root);
    if (depIt == payloadToDepIndex.end())
      return WalkResult::advance();
    unsigned depIdx = depIt->second;
    unsigned ownerDimCount = depOwnerDimCounts[depIdx];
    if (ownerDimCount == 0)
      return WalkResult::advance();
    if (indices.size() < ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }
    builder.setInsertionPoint(op);
    if (depHasGroupedBlocks(depIdx) || depRequiresDbRef[depIdx]) {
      if (root != memref) {
        op->emitError()
            << "grouped DB access through a memref view is not realized; "
               "SDE-to-ARTS must rewrite the view or fail closed";
        return WalkResult::interrupt();
      }
      auto sourceIt = payloadSources.find(root);
      if (sourceIt == payloadSources.end()) {
        op->emitError() << "has no grouped dependency source";
        return WalkResult::interrupt();
      }
      ArrayRef<Value> blockOffsets = depBlockOffsetArgs[depIdx];
      SmallVector<Value, 4> localBlockIndices;
      localBlockIndices.reserve(ownerDimCount);
      for (unsigned idx = 0; idx < ownerDimCount; ++idx) {
        Value local = arith::SubIOp::create(
            builder, op->getLoc(), indices[idx].get(), blockOffsets[idx]);
        localBlockIndices.push_back(local);
      }
      Value groupedPayload = arts::DbRefOp::create(
          builder, op->getLoc(), sourceIt->second, localBlockIndices);
      if (auto load = dyn_cast<memref::LoadOp>(op))
        load->setOperand(0, groupedPayload);
      else if (auto store = dyn_cast<memref::StoreOp>(op))
        store->setOperand(1, groupedPayload);
      else {
        op->emitError() << "unsupported grouped DB payload access";
        return WalkResult::interrupt();
      }
    }
    for (unsigned idx = 0; idx < ownerDimCount; ++idx)
      indices[idx].set(createZeroIndex(builder, op->getLoc()));
    return WalkResult::advance();
  };

  WalkResult result = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    }
    if (auto store = dyn_cast<memref::StoreOp>(op)) {
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    }
    return WalkResult::advance();
  });
  if (result.wasInterrupted())
    return failure();

  for (Value payload : payloads)
    if (Operation *op = payload.getDefiningOp())
      if (op->use_empty())
        op->erase();
  return result.wasInterrupted() ? failure() : success();
}

LogicalResult translateSdeAtomicsToArts(Region &region) {
  SmallVector<sde::SdeCuAtomicOp> atomics;
  region.walk([&](sde::SdeCuAtomicOp op) { atomics.push_back(op); });
  for (sde::SdeCuAtomicOp atomic : atomics) {
    if (atomic.getReductionKind() != sde::SdeReductionKind::add)
      return atomic.emitOpError()
             << "cannot realize non-add SDE atomic at the ARTS boundary";
    OpBuilder builder(atomic);
    arts::AtomicAddOp::create(builder, atomic.getLoc(), atomic.getAddr(),
                              atomic.getValue());
    atomic.erase();
  }
  return success();
}

LogicalResult
collectCloneableReadOnlyGlobalMemrefs(sde::SdeCuRegionOp source,
                                      const DenseSet<Value> &allowedDbHandles,
                                      SetVector<Value> &captures) {
  bool failed = false;
  source.getBody().walk([&](Operation *op) {
    if (isa<arts::DbAccessWindowOp, sde::SdeYieldOp>(op))
      return;
    for (Value operand : op->getOperands()) {
      if (!isa<MemRefType>(operand.getType()) ||
          isDefinedInside(operand, source.getOperation()))
        continue;
      if (allowedDbHandles.contains(operand))
        continue;
      if (operand.getDefiningOp<memref::GetGlobalOp>()) {
        if (!isReadOnlyMemrefUseInside(operand, source.getOperation())) {
          op->emitError()
              << "writes or escapes a cloned read-only memref.global inside a "
                 "standalone CU; mutable global state must be represented as "
                 "an explicit SDE/ARTS dependency";
          failed = true;
          continue;
        }
        captures.insert(operand);
        continue;
      }
      if (resolveBoundaryDbAlloc(operand)) {
        op->emitError()
            << "uses a DB-backed external memref that was not remapped to an "
               "EDT dependency; SDE must provide a committed access window for "
               "this standalone CU access";
        failed = true;
      }
    }
  });
  return failure(failed);
}
LogicalResult realizeStandaloneCuAccesses(sde::SdeCuRegionOp source) {
  if (!source || source->getParentOfType<sde::SdeSuIterateOp>())
    return success();

  SmallVector<DirectCuDepSpec, 4> deps;
  SmallVector<arts::DbAccessWindowOp, 4> windows;
  if (failed(collectStandaloneCuDependencies(source, deps, windows)))
    return failure();
  if (windows.empty())
    return success();

  if (source.getBody().empty())
    return source.emitOpError()
           << "has no body during standalone CU access realization";
  if (!source.getIterArgs().empty())
    return source.emitOpError()
           << "access-bearing standalone CU iter_args are not representable "
              "as an ARTS EDT; realize an explicit SDE dataflow first";
  for (Type resultType : source.getResultTypes())
    if (!isScalarParamType(resultType))
      return source.emitOpError()
             << "access-bearing standalone CU non-scalar results require an "
                "explicit SDE dataflow result before ARTS realization";
  Block &body = source.getBody().front();
  if (body.getNumArguments() != 0)
    return source.emitOpError()
           << "has region arguments during standalone CU EDT realization";
  OpBuilder builder(source.getContext());

  builder.setInsertionPoint(source);
  Location loc = source.getLoc();
  arts::ArtsLaunchPolicy standaloneLaunch;
  if (hasDistributedLaunchStorageFacts(deps)) {
    Value zero = createZeroIndex(builder, loc);
    standaloneLaunch = arts::resolveArtsOrdinalLaunchPolicy(
        source->getParentOfType<ModuleOp>(), zero, builder, loc);
  }
  Value standaloneRoute = standaloneLaunch.route
                              ? standaloneLaunch.route
                              : arts::createCurrentNodeRoute(builder, loc);

  for (DirectCuDepSpec &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    std::optional<arts::PartitionMode> partitionMode =
        std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    if (dep.ownerDimCount == 0) {
      buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
      partitionMode = arts::PartitionMode::coarse;
    } else {
      offsets.reserve(dep.ownerDimCount);
      sizes.reserve(dep.ownerDimCount);
      for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
        offsets.push_back(createConstantIndex(builder, loc, dep.blockLo[idx]));
        sizes.push_back(createConstantIndex(
            builder, loc, dep.blockHi[idx] - dep.blockLo[idx]));
      }
    }

    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.haloShape) {
      acquire.setDepPatternAttr(ArtsDepPatternAttr::get(
          source.getContext(), ArtsDepPattern::stencil));
      acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
          source.getContext(), EdtDistributionPattern::stencil));
    }
    dep.acquiredPtr = acquire.getPtr();
  }

  SmallVector<CuResultSpec, 4> resultSpecs;
  resultSpecs.reserve(source.getNumResults());
  for (Type resultType : source.getResultTypes()) {
    Value one = createOneIndex(builder, loc);
    Type payloadType = arts::getElementMemRefType(resultType, 1);
    Type pointerType = MemRefType::get({ShapedType::kDynamic}, payloadType);
    auto resultDb = arts::DbAllocOp::create(
        builder, loc, ArtsMode::inout, standaloneRoute, DbAllocType::heap,
        DbMode::write, resultType, pointerType, SmallVector<Value>{one},
        SmallVector<Value>{one}, arts::PartitionMode::coarse);
    auto writeAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::out, resultDb.getGuid(), resultDb.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{createZeroIndex(builder, loc)},
        SmallVector<Value>{one}, SmallVector<Value>{}, SmallVector<Value>{},
        SmallVector<Value>{}, Value{}, SmallVector<Value>{},
        SmallVector<Value>{});
    writeAcquire.setPreserveAccessMode();
    resultSpecs.push_back({resultDb, writeAcquire.getPtr(), Value{}});
  }

  DenseMap<Operation *, DirectCuDepSpec *> depsByAlloc;
  for (DirectCuDepSpec &dep : deps)
    depsByAlloc[dep.alloc.getOperation()] = &dep;

  auto rewriteAccess = [&](Operation *op, Value memref,
                           MutableOperandRange indices) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    auto it = depsByAlloc.find(alloc ? alloc.getOperation() : nullptr);
    if (it == depsByAlloc.end())
      return WalkResult::advance();

    DirectCuDepSpec &dep = *it->second;
    if (indices.size() < dep.ownerDimCount) {
      op->emitError() << "rank-expanded DB payload access has fewer indices "
                         "than committed owner dimensions";
      return WalkResult::interrupt();
    }

    builder.setInsertionPoint(op);
    SmallVector<Value, 4> localBlockIndices;
    localBlockIndices.reserve(dep.ownerDimCount);
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx) {
      Value local = indices[idx].get();
      if (dep.blockLo[idx] != 0)
        local = arith::SubIOp::create(
            builder, op->getLoc(), local,
            createConstantIndex(builder, op->getLoc(), dep.blockLo[idx]));
      localBlockIndices.push_back(local);
    }
    SmallVector<Value, 4> dbRefIndices;
    if (dep.ownerDimCount == 0)
      dbRefIndices.push_back(createZeroIndex(builder, op->getLoc()));
    else
      dbRefIndices.assign(localBlockIndices.begin(), localBlockIndices.end());
    Value payload = arts::DbRefOp::create(builder, op->getLoc(),
                                          dep.acquiredPtr, dbRefIndices);
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load->setOperand(0, payload);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store->setOperand(1, payload);
    else {
      op->emitError() << "unsupported direct DB payload access";
      return WalkResult::interrupt();
    }
    for (unsigned idx = 0; idx < dep.ownerDimCount; ++idx)
      indices[idx].set(createZeroIndex(builder, op->getLoc()));
    return WalkResult::advance();
  };

  WalkResult rewriteResult = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteAccess(op, load.getMemref(), load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteAccess(op, store.getMemref(), store.getIndicesMutable());
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  for (arts::DbAccessWindowOp window : windows)
    if (window && window->getBlock())
      window.erase();

  Operation *terminator = body.getTerminator();
  for (DirectCuDepSpec &dep : deps) {
    builder.setInsertionPoint(terminator ? terminator : &body.back());
    arts::DbReleaseOp::create(builder, loc, dep.acquiredPtr);
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();
  DenseSet<Value> allowedDbHandles;
  for (DirectCuDepSpec &dep : deps) {
    allowedDbHandles.insert(dep.acquiredPtr);
    allowedDbHandles.insert(dep.alloc.getPtr());
  }
  for (CuResultSpec &result : resultSpecs)
    allowedDbHandles.insert(result.writePtr);
  SetVector<Value> cloneableReadOnlyGlobalMemrefs;
  if (failed(collectCloneableReadOnlyGlobalMemrefs(
          source, allowedDbHandles, cloneableReadOnlyGlobalMemrefs)))
    return failure();

  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size() + resultSpecs.size());
  for (DirectCuDepSpec &dep : deps)
    taskDeps.push_back(dep.acquiredPtr);
  for (CuResultSpec &result : resultSpecs)
    taskDeps.push_back(result.writePtr);

  SmallVector<Value, 8> taskParams(scalarCaptures.begin(),
                                   scalarCaptures.end());

  builder.setInsertionPoint(source);
  arts::ArtsLaunchPolicy launch = standaloneLaunch;
  Value route = standaloneRoute;
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::sync, launch.concurrency,
                          route, taskDeps, taskParams);

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value depArg = taskBlock.getArgument(idx);
    mapper.map(dep.acquiredPtr, depArg);
    mapper.map(dep.alloc.getPtr(), depArg);
  }
  unsigned resultDepBase = deps.size();
  for (auto [idx, result] : llvm::enumerate(resultSpecs))
    mapper.map(result.writePtr, taskBlock.getArgument(resultDepBase + idx));
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  auto yield = dyn_cast_or_null<sde::SdeYieldOp>(body.getTerminator());
  if (source.getNumResults() != 0 &&
      (!yield || yield.getValues().size() != source.getNumResults()))
    return source.emitOpError()
           << "has mismatched yield/result count during standalone CU EDT "
              "realization";

  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (Value capture : cloneableReadOnlyGlobalMemrefs) {
    Operation *def = capture.getDefiningOp();
    Operation *cloned = def->clone(mapper);
    bodyBuilder.insert(cloned);
    mapper.map(capture, cloned->getResult(0));
  }
  for (Operation &nested : body) {
    if (isa<arts::DbAccessWindowOp, sde::SdeYieldOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  for (auto [idx, result] : llvm::enumerate(resultSpecs)) {
    Value zero = createZeroIndex(bodyBuilder, loc);
    Value payload =
        arts::DbRefOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx),
                              SmallVector<Value>{zero})
            .getResult();
    Value yielded = remapOrSelf(mapper, yield.getValues()[idx]);
    memref::StoreOp::create(bodyBuilder, loc, yielded, payload,
                            SmallVector<Value>{zero});
    arts::DbReleaseOp::create(bodyBuilder, loc,
                              taskBlock.getArgument(resultDepBase + idx));
  }
  arts::YieldOp::create(bodyBuilder, loc);

  builder.setInsertionPointAfter(task);
  for (auto [idx, result] : llvm::enumerate(resultSpecs)) {
    Value zero = createZeroIndex(builder, loc);
    Value one = createOneIndex(builder, loc);
    auto readAcquire = arts::DbAcquireOp::create(
        builder, loc, ArtsMode::in, result.alloc.getGuid(),
        result.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, SmallVector<Value>{zero}, SmallVector<Value>{one},
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    readAcquire.setPreserveAccessMode();
    Value payload = arts::DbRefOp::create(builder, loc, readAcquire.getPtr(),
                                          SmallVector<Value>{zero})
                        .getResult();
    Value loaded =
        memref::LoadOp::create(builder, loc, payload, SmallVector<Value>{zero});
    arts::DbReleaseOp::create(builder, loc, readAcquire.getPtr());
    result.replacement = loaded;
    source.getResult(idx).replaceAllUsesWith(loaded);
  }

  source.erase();
  return success();
}


LogicalResult
convertCoarseSuIterate(sde::SdeSuIterateOp source,
                       SmallVectorImpl<CoarseSuDependency> &deps) {
  if (deps.empty())
    return source.emitOpError()
           << "has no DB-backed accesses for coarse SDE-to-ARTS SU "
              "realization";
  if (sde::recoverCommittedPhysicalLayout(source)) {
    if (llvm::any_of(deps, [](CoarseSuDependency &dep) {
          auto partition = dep.alloc.getPartitionMode();
          return partition && *partition != arts::PartitionMode::coarse;
        }))
      return source.emitOpError()
             << "has committed physical partition facts but no access-window "
                "dependencies; refusing coarse ARTS realization";
  }
  if (source.getAccessMinOffsetsAttr() || source.getAccessMaxOffsetsAttr() ||
      source.getOwnerDimsAttr() || source.getSpatialDimsAttr() ||
      source.getWriteFootprintAttr())
    return source.emitOpError()
           << "has movement, halo, or physical scheduling facts without "
              "committed access windows; refusing coarse ARTS realization";

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (CoarseSuDependency &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        std::optional<arts::PartitionMode>(arts::PartitionMode::coarse),
        SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
        SmallVector<Value>{}, SmallVector<Value>{}, Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (CoarseSuDependency &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::sync,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (failed(attachUnpartitionedSdeFacts(source, task)))
    return failure();

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  SmallVector<Value, 4> payloads;
  payloads.reserve(deps.size());
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    payloads.push_back(payload);
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
  }

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
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return;
    auto it = llvm::find_if(deps, [&](const CoarseSuDependency &dep) {
      return dep.alloc == alloc;
    });
    if (it == deps.end())
      return;
    unsigned depIdx = static_cast<unsigned>(std::distance(deps.begin(), it));
    mapper.map(memref, payloads[depIdx]);
    Value root = ValueAnalysis::stripMemrefViewOps(memref);
    if (root && root != memref)
      mapper.map(root, payloads[depIdx]);
  });
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    Value lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }

  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessWindowOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(task);
    barrierBuilder.setInsertionPointAfter(task);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

LogicalResult
tryConvertCoarseSuIterate(sde::SdeSuIterateOp source) {
  SmallVector<CoarseSuDependency, 4> coarseDeps;
  if (failed(collectCoarseSuDependencies(source, coarseDeps)))
    return failure();
  return convertCoarseSuIterate(source, coarseDeps);
}

LogicalResult
convertSuIterate(sde::SdeSuIterateOp source,
                 DenseSet<Operation *> &consumedCuLevelAccessWindows,
                 SmallVectorImpl<Operation *> &consumedRedists) {
  if (source.getNumResults() != 0 || !source.getReductionAccumulators().empty())
    return source.emitOpError()
           << "direct SDE-to-ARTS lowering requires reduction/result facts to "
              "be authored as explicit SDE-to-ARTS reduction operations";

  SmallVector<DirectDepSpec, 4> deps;
  if (failed(collectSuDependencies(source, deps, consumedCuLevelAccessWindows,
                                   consumedRedists)))
    return failure();
  if (deps.empty()) {
    if (hasCommittedPartialReductionFacts(source))
      return source.emitOpError()
             << "commits partial-reduction facts without SDE access windows; "
                "SDE must expose block dependencies before ARTS lowering";
    return tryConvertCoarseSuIterate(source);
  }

  std::optional<CommittedPhysicalLayout> physicalLayout =
      readCommittedPhysicalLayout(source, deps);
  if (!physicalLayout || physicalLayout->blockShape.empty() ||
      physicalLayout->ownerDims.empty())
    return tryConvertCoarseSuIterate(source);

  ArrayRef<int64_t> ownerDims = physicalLayout->ownerDims;
  ArrayRef<int64_t> blockShape = physicalLayout->blockShape;

  unsigned loopRank = source.getUpperBounds().size();
  if (source.getLowerBounds().size() != loopRank ||
      source.getSteps().size() != loopRank ||
      source.getBody().front().getNumArguments() < loopRank)
    return source.emitOpError() << "has inconsistent loop bounds";

  FailureOr<ArtsOwnerSlotMapping> ownerRouteping = resolveArtsOwnerSlotMapping(
      ownerDims, blockShape, loopRank, source.getOperation());
  if (failed(ownerRouteping))
    return failure();

  ArrayRef<int64_t> ownerSlotDims = ownerRouteping->ownerDims;
  unsigned ownerDimCount = ownerSlotDims.size();
  for (DirectDepSpec &dep : deps)
    if (dep.accessSlots.size() != dep.ownerDimCount && !dep.reduceScatter)
      return source.emitOpError()
             << "dependency access-window coordinates do not match owner rank";

  SmallVector<int64_t, 4> ownerBlockSizes(ownerRouteping->blockSizes.begin(),
                                          ownerRouteping->blockSizes.end());

  SmallVector<int64_t, 4> workerSpans(ownerBlockSizes.begin(),
                                      ownerBlockSizes.end());
  SmallVector<int64_t, 4> groupBlockCounts(ownerDimCount, 1);
  if (sde::SdeCuRegionOp computeCu = sde::findSuComputeCuRegion(source)) {
    if (auto groupCounts =
            readI64ArrayAttr(computeCu.getGroupBlockCountAttr())) {
      if (groupCounts->size() != ownerDimCount)
        return source.emitOpError()
               << "commits groupBlockCount whose rank does not match the "
                  "committed owner rank";
      for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
        int64_t count = (*groupCounts)[slot];
        int64_t blockSize = ownerBlockSizes[slot];
        if (count <= 0)
          return source.emitOpError()
                 << "commits groupBlockCount that cannot be represented as "
                    "a whole-number group of physical DB blocks";
        workerSpans[slot] = blockSize * count;
        groupBlockCounts[slot] = count;
      }
    }
  }

  bool splitToOwnerLocalGroups = false;
  if (hasDistributedWriterStorageFacts(deps)) {
    std::optional<int64_t> totalNodes =
        arts::getRuntimeTotalNodes(source->getParentOfType<ModuleOp>());
    if (!totalNodes)
      return source.emitOpError()
             << "requires runtime node count to keep grouped distributed "
                "writers owner-local";
    if (failed(ensureDistributedWriterOwnerLocalGroups(
            source, deps, groupBlockCounts, workerSpans, ownerBlockSizes,
            *totalNodes, splitToOwnerLocalGroups)))
      return failure();
  }

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  Block *computeBlock = sde::getSuIterateComputeBlock(source);
  if (!computeBlock)
    return source.emitOpError() << "has no computable body";

  SmallVector<CompactHaloColumnSpec, 4> compactHaloColumnSpecs;
  DenseMap<unsigned, unsigned> compactColumnSpecByDepIndex;
  SmallVector<CompactHaloNdSpec, 4> compactHaloNdSpecs;
  DenseMap<unsigned, unsigned> compactNdSpecByDepIndex;
  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    if (!dep.haloShape)
      continue;
    if (dep.mode != ArtsMode::in)
      return source.emitOpError()
             << "commits a halo dependency that is not read-only; ARTS cannot "
                "realize a writable halo window";
    bool useExactNdHalo = false;
    if (dep.ownerDimCount == 2) {
      FailureOr<bool> needsExact =
          needsExactNdHaloFor2D(source, dep, computeBlock);
      if (failed(needsExact))
        return failure();
      useExactNdHalo = *needsExact;
    }
    if (dep.ownerDimCount == 2 && !useExactNdHalo) {
      FailureOr<CompactHaloColumnSpec> compactSpec =
          realizeCompactHaloColumnPacks(source, dep, groupBlockCounts, builder,
                                        loc);
      if (failed(compactSpec))
        return failure();
      compactColumnSpecByDepIndex[depIndex] =
          static_cast<unsigned>(compactHaloColumnSpecs.size());
      compactHaloColumnSpecs.push_back(*compactSpec);
      continue;
    }
    FailureOr<CompactHaloNdSpec> compactSpec =
        realizeCompactHaloNdPacks(source, dep, groupBlockCounts, builder, loc);
    if (failed(compactSpec))
      return failure();
    compactNdSpecByDepIndex[depIndex] =
        static_cast<unsigned>(compactHaloNdSpecs.size());
    compactHaloNdSpecs.push_back(*compactSpec);
  }
  if (!compactHaloColumnSpecs.empty() || !compactHaloNdSpecs.empty()) {
    auto reason = arts::ArtsBarrierReasonAttr::get(
        source.getContext(), arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(builder, loc, reason);
  }

  SmallVector<Value, 4> dispatchBases;
  SmallVector<Value, 4> dispatchBlockOffsets;
  scf::ForOp dispatchRoot;
  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerRouteping->loopDims[slot];
    Value step = createConstantIndex(builder, loc, workerSpans[slot]);
    auto loop =
        scf::ForOp::create(builder, loc, source.getLowerBounds()[physicalDim],
                           source.getUpperBounds()[physicalDim], step);
    if (!dispatchRoot)
      dispatchRoot = loop;
    dispatchBases.push_back(loop.getInductionVar());
    builder.setInsertionPointToStart(loop.getBody());
  }

  for (unsigned slot = 0; slot < ownerDimCount; ++slot) {
    unsigned physicalDim = ownerRouteping->loopDims[slot];
    Value base = dispatchBases[slot];
    Value lower = source.getLowerBounds()[physicalDim];
    Value delta = arith::SubIOp::create(builder, loc, base, lower);
    Value blockSize = createConstantIndex(builder, loc, ownerBlockSizes[slot]);
    dispatchBlockOffsets.push_back(
        arith::DivUIOp::create(builder, loc, delta, blockSize));
  }
  DenseMap<unsigned, unsigned> dispatchSlotByLoopDim;
  for (auto [slot, loopDim] : llvm::enumerate(ownerRouteping->loopDims))
    dispatchSlotByLoopDim.try_emplace(loopDim, static_cast<unsigned>(slot));

  unsigned ndHaloSideCount = 0;
  for (const CompactHaloNdSpec &spec : compactHaloNdSpecs)
    ndHaloSideCount += spec.sides.size();
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size() + compactHaloColumnSpecs.size() * 4 +
                   ndHaloSideCount);
  SmallVector<Attribute, 4> partialReductionDepResultDimMaps;
  const bool hasPartialReduction = hasCommittedPartialReductionFacts(source);
  if (hasPartialReduction)
    partialReductionDepResultDimMaps.reserve(taskDeps.capacity());
  SmallVector<SmallVector<Value, 4>> depBlockOffsets;
  SmallVector<bool> depRequiresDbRef;
  SmallVector<unsigned, 4> taskDepOwnerDimCounts;
  SmallVector<SmallVector<int64_t, 4>> taskDepGroupBlockCounts;
  SmallVector<unsigned, 4> primaryTaskDepForDep(deps.size(), 0);
  SmallVector<Halo2DTaskWork, 4> haloTaskWorks;
  DenseMap<unsigned, unsigned> haloTaskWorkByDepIndex;
  SmallVector<HaloNdTaskWork, 4> haloNdTaskWorks;
  DenseMap<unsigned, unsigned> haloNdTaskWorkByDepIndex;
  depBlockOffsets.reserve(taskDeps.capacity());
  depRequiresDbRef.reserve(taskDeps.capacity());
  taskDepOwnerDimCounts.reserve(taskDeps.capacity());
  taskDepGroupBlockCounts.reserve(taskDeps.capacity());

  auto appendTaskDep = [&](Value depPtr, ArrayRef<Value> blockOffsets,
                           bool requiresDbRef, unsigned depOwnerDimCount,
                           ArrayRef<int64_t> depGroupBlockCounts,
                           ArrayAttr depResultDimMap) -> unsigned {
    unsigned taskDepIndex = static_cast<unsigned>(taskDeps.size());
    taskDeps.push_back(depPtr);
    if (hasPartialReduction)
      partialReductionDepResultDimMaps.push_back(
          depResultDimMap ? depResultDimMap
                          : Builder(source.getContext()).getArrayAttr({}));
    depBlockOffsets.push_back(
        SmallVector<Value, 4>(blockOffsets.begin(), blockOffsets.end()));
    depRequiresDbRef.push_back(requiresDbRef);
    taskDepOwnerDimCounts.push_back(depOwnerDimCount);
    taskDepGroupBlockCounts.push_back(SmallVector<int64_t, 4>(
        depGroupBlockCounts.begin(), depGroupBlockCounts.end()));
    return taskDepIndex;
  };

  for (auto [depIndex, dep] : llvm::enumerate(deps)) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    SmallVector<int64_t, 4> depGroupBlockCounts(dep.ownerDimCount, 1);
    offsets.reserve(dep.ownerDimCount);
    sizes.reserve(dep.ownerDimCount);

    if (dep.reduceScatter) {
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        offsets.push_back(createZeroIndex(builder, loc));
        sizes.push_back(dep.alloc.getSizes()[slot]);
      }
    } else {
      if (dep.accessSlots.size() != dep.ownerDimCount)
        return source.emitOpError() << "dependency access-window coordinates "
                                       "do not cover owner rank";
      for (unsigned slot = 0; slot < dep.ownerDimCount; ++slot) {
        const DepOwnerAccessSlot &access = dep.accessSlots[slot];
        if (access.fullWindow) {
          Value offset = createConstantIndex(builder, loc, dep.blockLo[slot]);
          int64_t staticGroupCount =
              std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
          Value requested = createConstantIndex(builder, loc, staticGroupCount);
          Value remaining = arith::SubIOp::create(
              builder, loc, dep.alloc.getSizes()[slot], offset);
          offsets.push_back(offset);
          sizes.push_back(
              arith::MinUIOp::create(builder, loc, remaining, requested));
          depGroupBlockCounts[slot] = staticGroupCount;
          continue;
        }
        if (access.fixedBlock) {
          offsets.push_back(
              createConstantIndex(builder, loc, *access.fixedBlock));
          sizes.push_back(createOneIndex(builder, loc));
          depGroupBlockCounts[slot] = 1;
          continue;
        }
        if (!access.loopDim ||
            *access.loopDim >= source.getUpperBounds().size())
          return source.emitOpError()
                 << "dependency access-window coordinate has no loop dimension";
        unsigned physicalDim = *access.loopDim;
        FailureOr<unsigned> depPayloadDim =
            getAccessWindowPayloadDim(source, dep, slot, physicalDim);
        if (failed(depPayloadDim))
          return failure();
        FailureOr<int64_t> payloadExtent =
            getAccessWindowPayloadExtent(source, dep, *depPayloadDim);
        if (failed(payloadExtent))
          return failure();
        (void)payloadExtent;
        int64_t coordinateBlockSize = access.coordinateBlockSize;
        if (coordinateBlockSize <= 0)
          return source.emitOpError()
                 << "dependency access-window coordinate has non-positive "
                    "block size";
        Value lower = source.getLowerBounds()[physicalDim];
        Value upper = source.getUpperBounds()[physicalDim];
        Value coordinateBlockSizeValue =
            createConstantIndex(builder, loc, coordinateBlockSize);
        Value base = lower;
        Value groupEnd = upper;
        int64_t staticGroupCount =
            std::max<int64_t>(1, dep.blockHi[slot] - dep.blockLo[slot]);
        auto dispatchIt = dispatchSlotByLoopDim.find(physicalDim);
        if (dispatchIt != dispatchSlotByLoopDim.end()) {
          unsigned dispatchSlot = dispatchIt->second;
          base = dispatchBases[dispatchSlot];
          Value groupSpan =
              createConstantIndex(builder, loc, workerSpans[dispatchSlot]);
          groupEnd = arith::MinUIOp::create(
              builder, loc,
              arith::AddIOp::create(builder, loc, base, groupSpan), upper);
          staticGroupCount =
              std::max<int64_t>(1, ceilDivPositiveI64(workerSpans[dispatchSlot],
                                                      coordinateBlockSize));
        }
        Value rawOffset = arith::DivUIOp::create(builder, loc, base,
                                                 coordinateBlockSizeValue);
        Value rawEnd = ceilDivPositiveIndex(builder, loc, groupEnd,
                                            coordinateBlockSizeValue);
        Value offset = rawOffset;
        if (dep.blockLo[slot] != 0) {
          Value windowLo = createConstantIndex(builder, loc, dep.blockLo[slot]);
          offset = arith::MaxUIOp::create(builder, loc, rawOffset, windowLo);
        }
        Value windowHi = createConstantIndex(builder, loc, dep.blockHi[slot]);
        Value end = arith::MinUIOp::create(builder, loc, rawEnd, windowHi);
        Value remaining = arith::SubIOp::create(
            builder, loc, dep.alloc.getSizes()[slot], offset);
        Value count = arith::SubIOp::create(builder, loc, end, offset);
        sizes.push_back(arith::MinUIOp::create(builder, loc, remaining, count));
        offsets.push_back(offset);
        depGroupBlockCounts[slot] = staticGroupCount;
      }
    }
    bool requiresDbRef = dep.reduceScatter.has_value() ||
                         llvm::any_of(depGroupBlockCounts,
                                      [](int64_t count) { return count > 1; });
    FailureOr<ArrayAttr> depResultDimMap =
        buildPartialReductionDepResultDimMap(source, dep);
    if (failed(depResultDimMap))
      return failure();

    if (dep.haloShape) {
      auto centerAcquire = arts::DbAcquireOp::create(
          builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
          std::optional<arts::PartitionMode>(arts::PartitionMode::block),
          SmallVector<Value>{}, offsets, sizes, SmallVector<Value>{},
          SmallVector<Value>{}, SmallVector<Value>{}, Value{},
          SmallVector<Value>{}, SmallVector<Value>{});
      centerAcquire.setPreserveAccessMode();
      unsigned centerTaskDep = appendTaskDep(
          centerAcquire.getPtr(), offsets, requiresDbRef, dep.ownerDimCount,
          depGroupBlockCounts, *depResultDimMap);
      primaryTaskDepForDep[depIndex] = centerTaskDep;

      auto columnSpecIt = compactColumnSpecByDepIndex.find(depIndex);
      if (columnSpecIt != compactColumnSpecByDepIndex.end()) {
        const CompactHaloColumnSpec &compactSpec =
            compactHaloColumnSpecs[columnSpecIt->second];

        SmallVector<Value, 4> topOffsets;
        auto topAcquire = create2DUnitRowHaloAcquire(
            source, dep, dispatchBlockOffsets, compactSpec, /*topFace=*/true,
            topOffsets, builder, loc);
        unsigned topTaskDep = appendTaskDep(
            topAcquire.getPtr(), topOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        SmallVector<Value, 4> bottomOffsets;
        auto bottomAcquire = create2DUnitRowHaloAcquire(
            source, dep, dispatchBlockOffsets, compactSpec, /*topFace=*/false,
            bottomOffsets, builder, loc);
        unsigned bottomTaskDep = appendTaskDep(
            bottomAcquire.getPtr(), bottomOffsets, /*requiresDbRef=*/false,
            dep.ownerDimCount, SmallVector<int64_t, 4>(dep.ownerDimCount, 1),
            ArrayAttr{});

        SmallVector<Value, 4> leftOffsets;
        auto leftAcquire = create2DUnitCompactColumnAcquire(
            dep, dispatchBlockOffsets, compactSpec, /*leftFace=*/true,
            leftOffsets, builder, loc);
        unsigned leftTaskDep = appendTaskDep(
            leftAcquire.getPtr(), leftOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        SmallVector<Value, 4> rightOffsets;
        auto rightAcquire = create2DUnitCompactColumnAcquire(
            dep, dispatchBlockOffsets, compactSpec, /*leftFace=*/false,
            rightOffsets, builder, loc);
        unsigned rightTaskDep = appendTaskDep(
            rightAcquire.getPtr(), rightOffsets,
            /*requiresDbRef=*/false, dep.ownerDimCount,
            SmallVector<int64_t, 4>(dep.ownerDimCount, 1), ArrayAttr{});

        Halo2DTaskWork haloTaskWork;
        haloTaskWork.centerTaskDepIndex = centerTaskDep;
        haloTaskWork.topTaskDepIndex = topTaskDep;
        haloTaskWork.bottomTaskDepIndex = bottomTaskDep;
        haloTaskWork.leftTaskDepIndex = leftTaskDep;
        haloTaskWork.rightTaskDepIndex = rightTaskDep;
        haloTaskWork.rowExtent = compactSpec.rowExtent;
        haloTaskWork.colExtent = compactSpec.colExtent;
        haloTaskWorkByDepIndex[depIndex] =
            static_cast<unsigned>(haloTaskWorks.size());
        haloTaskWorks.push_back(haloTaskWork);
        continue;
      }

      auto specIt = compactNdSpecByDepIndex.find(depIndex);
      if (specIt == compactNdSpecByDepIndex.end())
        return source.emitOpError()
               << "lost compact N-D halo payload state for committed halo "
                  "dependency";
      const CompactHaloNdSpec &compactSpec = compactHaloNdSpecs[specIt->second];
      HaloNdTaskWork haloTaskWork;
      haloTaskWork.centerTaskDepIndex = centerTaskDep;
      haloTaskWork.elementExtents.assign(compactSpec.elementExtents.begin(),
                                         compactSpec.elementExtents.end());
      SmallVector<int64_t, 4> singleBlockCounts(dep.ownerDimCount, 1);
      for (const CompactHaloNdSideSpec &side : compactSpec.sides) {
        SmallVector<Value, 4> sideOffsets;
        auto sideAcquire = createNdCompactHaloAcquire(
            dep, dispatchBlockOffsets, side, sideOffsets, builder, loc);
        unsigned sideTaskDep =
            appendTaskDep(sideAcquire.getPtr(), sideOffsets,
                          /*requiresDbRef=*/false, dep.ownerDimCount,
                          singleBlockCounts, ArrayAttr{});
        haloTaskWork.sideSourceOffsets.push_back(side.sourceOffsets);
        haloTaskWork.sideTaskDepIndices.push_back(sideTaskDep);
      }
      haloNdTaskWorkByDepIndex[depIndex] =
          static_cast<unsigned>(haloNdTaskWorks.size());
      haloNdTaskWorks.push_back(std::move(haloTaskWork));
      continue;
    }

    std::optional<arts::PartitionMode> partitionMode =
        std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    if (dep.ownerDimCount == 0) {
      buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
      partitionMode = arts::PartitionMode::coarse;
    }
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{}, SmallVector<Value>{}, SmallVector<Value>{},
        Value{}, SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    if (dep.reduceScatter)
      acquire.setReplicatedReadAttr(UnitAttr::get(source.getContext()));
    primaryTaskDepForDep[depIndex] =
        appendTaskDep(acquire.getPtr(), offsets, requiresDbRef,
                      dep.ownerDimCount, depGroupBlockCounts, *depResultDimMap);
  }

  SmallVector<Value, 8> taskParams;
  taskParams.append(dispatchBases.begin(), dispatchBases.end());
  taskParams.append(dispatchBlockOffsets.begin(), dispatchBlockOffsets.end());

  auto appendParamIfMissing = [&](Value value) -> unsigned {
    auto it = llvm::find(taskParams, value);
    if (it != taskParams.end())
      return static_cast<unsigned>(std::distance(taskParams.begin(), it));
    taskParams.push_back(value);
    return taskParams.size() - 1;
  };

  SmallVector<SmallVector<unsigned, 4>> depBlockOffsetParamIndices;
  depBlockOffsetParamIndices.reserve(depBlockOffsets.size());
  for (ArrayRef<Value> offsets : depBlockOffsets) {
    SmallVector<unsigned, 4> paramIndices;
    paramIndices.reserve(ownerDimCount);
    for (Value offset : offsets)
      paramIndices.push_back(appendParamIfMissing(offset));
    depBlockOffsetParamIndices.push_back(std::move(paramIndices));
  }

  for (Value capture : scalarCaptures)
    appendParamIfMissing(capture);

  arts::ArtsLaunchPolicy launch = arts::resolveArtsLaunchPolicy(
      source->getParentOfType<ModuleOp>(), dispatchRoot,
      hasDistributedLaunchStorageFacts(deps), builder, loc);
  Value route =
      launch.route ? launch.route : arts::createCurrentNodeRoute(builder, loc);
  auto task =
      arts::EdtOp::create(builder, loc, arts::EdtType::task, launch.concurrency,
                          route, taskDeps, taskParams);
  if (failed(attachCommittedSdeFacts(source, task)))
    return failure();
  if (hasPartialReduction) {
    if (partialReductionDepResultDimMaps.size() != taskDeps.size())
      return source.emitOpError()
             << "lost partial-reduction dependency/result mapping while "
                "building ARTS task dependencies";
    task.setPartialReductionDepResultDimMapsAttr(
        builder.getArrayAttr(partialReductionDepResultDimMaps));
  }
  if (splitToOwnerLocalGroups)
    task.setOwnerLocalWriterSplitAttr(UnitAttr::get(source.getContext()));

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  SmallVector<Value, 4> payloads;
  payloads.reserve(taskDeps.size());
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(taskDeps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    payloads.push_back(payload);
  }
  auto mapIfAbsent = [&](Value from, Value to) {
    if (from && !mapper.lookupOrNull(from))
      mapper.map(from, to);
  };
  for (auto [depIdx, dep] : llvm::enumerate(deps)) {
    unsigned taskDepIndex = primaryTaskDepForDep[depIdx];
    mapIfAbsent(dep.alloc.getPtr(), taskBlock.getArgument(taskDepIndex));
  }
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, base] : llvm::enumerate(dispatchBases))
    mapper.map(base, taskBlock.getArgument(paramOffset + idx));
  for (auto [idx, offset] : llvm::enumerate(dispatchBlockOffsets))
    mapper.map(offset,
               taskBlock.getArgument(paramOffset + ownerDimCount + idx));
  SmallVector<SmallVector<Value, 4>> taskDepBlockOffsetArgs;
  taskDepBlockOffsetArgs.reserve(depBlockOffsets.size());
  for (unsigned depIdx = 0; depIdx < depBlockOffsets.size(); ++depIdx) {
    SmallVector<Value, 4> offsets;
    unsigned depOwnerDimCount = taskDepOwnerDimCounts[depIdx];
    offsets.reserve(depOwnerDimCount);
    for (unsigned slot = 0; slot < depOwnerDimCount; ++slot) {
      unsigned paramIndex = depBlockOffsetParamIndices[depIdx][slot];
      offsets.push_back(taskBlock.getArgument(paramOffset + paramIndex));
    }
    taskDepBlockOffsetArgs.push_back(std::move(offsets));
  }

  WalkResult accessWindowMapResult =
      source.getBody().walk([&](arts::DbAccessWindowOp window) {
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(window.getMu()));
        if (!alloc) {
          window.emitOpError() << "lost backing DB allocation during direct "
                                  "SDE-to-ARTS lowering";
          return WalkResult::interrupt();
        }
        auto it = llvm::find_if(
            deps, [&](const DirectDepSpec &dep) { return dep.alloc == alloc; });
        if (it == deps.end()) {
          window.emitOpError() << "has no matching direct ARTS dependency";
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
  if (accessWindowMapResult.wasInterrupted())
    return failure();

  DenseMap<Operation *, HaloLoadRewrite> originalHaloLoadRewrites;
  if (!haloTaskWorks.empty()) {
    if (ownerDimCount != 2 || ownerRouteping->loopDims.size() != 2)
      return source.emitOpError() << "commits a halo dependency whose owner "
                                     "rank is not supported by "
                                     "ARTS compact 2D unit-halo load rewriting";
    WalkResult classifyResult =
        computeBlock->walk([&](memref::LoadOp load) {
          arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
          if (!alloc)
            return WalkResult::advance();
          auto depIt = llvm::find_if(deps, [&](const DirectDepSpec &dep) {
            return dep.alloc == alloc;
          });
          if (depIt == deps.end())
            return WalkResult::advance();
          unsigned depIdx =
              static_cast<unsigned>(std::distance(deps.begin(), depIt));
          auto haloIt = haloTaskWorkByDepIndex.find(depIdx);
          if (haloIt == haloTaskWorkByDepIndex.end())
            return WalkResult::advance();

          SmallVector<Value, 4> loopIvs;
          for (Operation *parent = load->getParentOp(); parent;
               parent = parent->getParentOp())
            if (auto loop = dyn_cast<scf::ForOp>(parent))
              loopIvs.push_back(loop.getInductionVar());
          if (loopIvs.size() < 2) {
            load.emitOpError()
                << "is not nested in the 2D compute loops required for ARTS "
                   "compact unit-halo load rewriting";
            return WalkResult::interrupt();
          }
          Value rowIv = loopIvs[1];
          Value colIv = loopIvs[0];
          FailureOr<std::optional<HaloLoadRewrite>> rewrite =
              classify2DUnitHaloLoad(load, haloIt->second,
                                     haloTaskWorks[haloIt->second], rowIv,
                                     colIv);
          if (failed(rewrite))
            return WalkResult::interrupt();
          if (rewrite->has_value())
            originalHaloLoadRewrites[load.getOperation()] = **rewrite;
          return WalkResult::advance();
        });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  DenseMap<Operation *, HaloNdLoadRewrite> originalHaloNdLoadRewrites;
  if (!haloNdTaskWorks.empty()) {
    WalkResult classifyResult =
        computeBlock->walk([&](memref::LoadOp load) {
          arts::DbAllocOp alloc = resolveBoundaryDbAlloc(load.getMemref());
          if (!alloc)
            return WalkResult::advance();
          auto depIt = llvm::find_if(deps, [&](const DirectDepSpec &dep) {
            return dep.alloc == alloc;
          });
          if (depIt == deps.end())
            return WalkResult::advance();
          unsigned depIdx =
              static_cast<unsigned>(std::distance(deps.begin(), depIt));
          auto haloIt = haloNdTaskWorkByDepIndex.find(depIdx);
          if (haloIt == haloNdTaskWorkByDepIndex.end())
            return WalkResult::advance();

          const HaloNdTaskWork &work = haloNdTaskWorks[haloIt->second];
          unsigned rank = work.elementExtents.size();
          SmallVector<Value, 4> loopIvs;
          for (Operation *parent = load->getParentOp(); parent;
               parent = parent->getParentOp())
            if (auto loop = dyn_cast<scf::ForOp>(parent))
              loopIvs.push_back(loop.getInductionVar());
          if (loopIvs.size() < rank) {
            load.emitOpError()
                << "is not nested in the N-D compute loops required for ARTS "
                   "compact unit-halo load rewriting";
            return WalkResult::interrupt();
          }

          SmallVector<Value, 4> ownerLoopIvs;
          ownerLoopIvs.reserve(rank);
          for (unsigned slot = 0; slot < rank; ++slot)
            ownerLoopIvs.push_back(loopIvs[rank - 1 - slot]);
          FailureOr<std::optional<HaloNdLoadRewrite>> rewrite =
              classifyNdUnitHaloLoad(load, haloIt->second, work, ownerLoopIvs);
          if (failed(rewrite))
            return WalkResult::interrupt();
          if (rewrite->has_value())
            originalHaloNdLoadRewrites[load.getOperation()] = **rewrite;
          return WalkResult::advance();
        });
    if (classifyResult.wasInterrupted())
      return failure();
  }

  for (unsigned dim = 0; dim < loopRank; ++dim) {
    auto ownerIt = llvm::find(ownerRouteping->loopDims, dim);
    Value lower;
    Value upper = remapOrSelf(mapper, source.getUpperBounds()[dim]);
    Value step = remapOrSelf(mapper, source.getSteps()[dim]);
    if (ownerIt != ownerRouteping->loopDims.end()) {
      unsigned slot = static_cast<unsigned>(
          std::distance(ownerRouteping->loopDims.begin(), ownerIt));
      lower = mapper.lookup(dispatchBases[slot]);
      Value localEnd = arith::AddIOp::create(
          bodyBuilder, loc, lower,
          createConstantIndex(bodyBuilder, loc, workerSpans[slot]));
      upper = arith::MinUIOp::create(bodyBuilder, loc, localEnd, upper);
    } else {
      lower = remapOrSelf(mapper, source.getLowerBounds()[dim]);
    }
    auto localLoop = scf::ForOp::create(bodyBuilder, loc, lower, upper, step);
    mapper.map(source.getBody().front().getArgument(dim),
               localLoop.getInductionVar());
    bodyBuilder.setInsertionPointToStart(localLoop.getBody());
  }

  DenseMap<Operation *, HaloLoadRewrite> clonedHaloLoadRewrites;
  DenseMap<Operation *, HaloNdLoadRewrite> clonedHaloNdLoadRewrites;
  for (Operation &nested : computeBlock->without_terminator()) {
    if (isa<arts::DbAccessWindowOp>(&nested))
      continue;
    Operation *cloned = nested.clone(mapper);
    bodyBuilder.insert(cloned);
    if (!originalHaloLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloLoadRewrites,
                                              clonedHaloLoadRewrites)))
        return failure();
    if (!originalHaloNdLoadRewrites.empty())
      if (failed(recordClonedHaloLoadRewrites(&nested, cloned,
                                              originalHaloNdLoadRewrites,
                                              clonedHaloNdLoadRewrites)))
        return failure();
  }

  auto rewriteClonedAccess = [&](Operation *op, Value memref,
                                 ArtsMode mode) -> WalkResult {
    arts::DbAllocOp alloc = resolveBoundaryDbAlloc(memref);
    if (!alloc)
      return WalkResult::advance();
    std::optional<unsigned> depIdx =
        findDirectDepIndexForAccess(deps, alloc, mode,
                                    /*preferHaloRead=*/true);
    if (!depIdx) {
      op->emitError()
          << "has no committed SDE access-window dependency for direct "
             "ARTS lowering";
      return WalkResult::interrupt();
    }
    unsigned taskDepIndex = primaryTaskDepForDep[*depIdx];
    if (taskDepIndex >= payloads.size()) {
      op->emitError() << "lost direct dependency payload while lowering "
                         "SDE access window";
      return WalkResult::interrupt();
    }
    if (auto load = dyn_cast<memref::LoadOp>(op))
      load.getMemrefMutable().assign(payloads[taskDepIndex]);
    else if (auto store = dyn_cast<memref::StoreOp>(op))
      store.getMemrefMutable().assign(payloads[taskDepIndex]);
    return WalkResult::advance();
  };
  WalkResult rewriteResult = task.getBody().walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteClonedAccess(op, load.getMemref(), ArtsMode::in);
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteClonedAccess(op, store.getMemref(), ArtsMode::out);
    return WalkResult::advance();
  });
  if (rewriteResult.wasInterrupted())
    return failure();

  if (failed(rewriteCloned2DUnitHaloLoads(task, clonedHaloLoadRewrites,
                                          haloTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(rewriteClonedNdUnitHaloLoads(task, clonedHaloNdLoadRewrites,
                                          haloNdTaskWorks, payloads,
                                          taskDepBlockOffsetArgs)))
    return failure();
  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  if (failed(rewriteOwnerIndicesToLocal(task, payloads, taskDepBlockOffsetArgs,
                                        depRequiresDbRef, taskDepOwnerDimCounts,
                                        taskDepGroupBlockCounts)))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  bool needsCompletionBarrier = !source.getNowaitAttr();
  MLIRContext *ctx = source.getContext();
  source.erase();
  if (needsCompletionBarrier) {
    OpBuilder barrierBuilder(dispatchRoot);
    barrierBuilder.setInsertionPointAfter(dispatchRoot);
    auto reason = arts::ArtsBarrierReasonAttr::get(
        ctx, arts::ArtsBarrierReason::required_memory);
    arts::BarrierOp::create(barrierBuilder, loc, reason);
  }
  return success();
}

LogicalResult
collectTaskDependencies(sde::SdeCuTaskOp source,
                        SmallVectorImpl<TaskDepSpec> &deps) {
  WalkResult result =
      source.getBody().walk([&](sde::SdeMuDepOp dep) {
        if (!dep.getDep().use_empty()) {
          dep.emitOpError()
              << "result is consumed; SDE task dependencies must remain local "
                 "declarations before ARTS realization";
          return WalkResult::interrupt();
        }
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts::DbUtils::getUnderlyingDbAlloc(dep.getSource()));
        if (!alloc) {
          dep.emitOpError()
              << "does not reference an ARTS DB-backed memref after storage "
                 "realization";
          return WalkResult::interrupt();
        }
        FailureOr<ArtsMode> mode = convertAccessMode(dep.getMode(), dep);
        if (failed(mode))
          return WalkResult::interrupt();
        deps.push_back({dep, alloc, *mode,
                        SmallVector<Value, 4>(dep.getOffsets().begin(),
                                              dep.getOffsets().end()),
                        SmallVector<Value, 4>(dep.getSizes().begin(),
                                              dep.getSizes().end())});
        return WalkResult::advance();
      });
  return result.wasInterrupted() ? failure() : success();
}

LogicalResult convertCuTask(sde::SdeCuTaskOp source) {
  SmallVector<TaskDepSpec, 4> deps;
  if (failed(collectTaskDependencies(source, deps)))
    return failure();

  SetVector<Value> scalarCaptures;
  if (failed(collectExternalScalarCaptures(source, scalarCaptures)))
    return failure();

  Location loc = source.getLoc();
  OpBuilder builder(source);
  SmallVector<Value, 4> taskDeps;
  taskDeps.reserve(deps.size());
  for (TaskDepSpec &dep : deps) {
    SmallVector<Value> offsets;
    SmallVector<Value> sizes;
    buildWholeDbAcquireWindow(builder, loc, dep.alloc, offsets, sizes);
    std::optional<arts::PartitionMode> partitionMode =
        dep.offsets.empty() && dep.sizes.empty()
            ? std::optional<arts::PartitionMode>(arts::PartitionMode::coarse)
            : std::optional<arts::PartitionMode>(arts::PartitionMode::block);
    auto acquire = arts::DbAcquireOp::create(
        builder, loc, dep.mode, dep.alloc.getGuid(), dep.alloc.getPtr(),
        partitionMode, SmallVector<Value>{}, offsets, sizes,
        SmallVector<Value>{},
        SmallVector<Value>(dep.offsets.begin(), dep.offsets.end()),
        SmallVector<Value>(dep.sizes.begin(), dep.sizes.end()), Value{},
        SmallVector<Value>{}, SmallVector<Value>{});
    acquire.setPreserveAccessMode();
    acquire.setPreserveDepEdge();
    taskDeps.push_back(acquire.getPtr());
  }

  SmallVector<Value, 8> taskParams;
  for (Value capture : scalarCaptures)
    taskParams.push_back(capture);
  auto appendParamIfMissing = [&](Value value) {
    if (!value || !isScalarParamType(value.getType()) ||
        isConstantLikeValue(value) || llvm::is_contained(taskParams, value))
      return;
    taskParams.push_back(value);
  };
  for (TaskDepSpec &dep : deps) {
    for (Value size : dep.alloc.getSizes())
      appendParamIfMissing(size);
    for (Value elementSize : dep.alloc.getElementSizes())
      appendParamIfMissing(elementSize);
  }

  Value route = arts::createCurrentNodeRoute(builder, loc);
  auto task = arts::EdtOp::create(builder, loc, arts::EdtType::task,
                                  arts::EdtConcurrency::intranode, route,
                                  taskDeps, taskParams);
  if (auto pattern = source.getPatternAttr()) {
    FailureOr<ArtsDepPattern> depPattern =
        convertPattern(pattern.getValue(), source.getOperation());
    if (failed(depPattern))
      return failure();
    arts::setDepPattern(task.getOperation(), *depPattern);
  }

  Block &taskBlock = task.getBody().front();
  for (Value dep : taskDeps)
    taskBlock.addArgument(dep.getType(), loc);
  unsigned paramOffset = taskBlock.getNumArguments();
  for (Value param : taskParams)
    taskBlock.addArgument(param.getType(), loc);

  IRMapping mapper;
  OpBuilder bodyBuilder(task.getContext());
  bodyBuilder.setInsertionPointToStart(&taskBlock);
  for (auto [idx, dep] : llvm::enumerate(deps)) {
    Value payload = arts::realizeDbInnerPayload(bodyBuilder, loc,
                                                taskBlock.getArgument(idx));
    mapper.map(dep.alloc.getPtr(), taskBlock.getArgument(idx));
    Value sourceMemref = dep.dep.getSource();
    mapper.map(sourceMemref, payload);
    if (Value root = ValueAnalysis::stripMemrefViewOps(sourceMemref))
      mapper.map(root, payload);
  }
  for (auto [idx, param] : llvm::enumerate(taskParams))
    mapper.map(param, taskBlock.getArgument(paramOffset + idx));

  for (Operation &nested : source.getBody().front()) {
    if (isa<sde::SdeMuDepOp>(&nested))
      continue;
    bodyBuilder.insert(nested.clone(mapper));
  }

  if (failed(translateSdeAtomicsToArts(task.getBody())))
    return failure();
  bodyBuilder.setInsertionPointToEnd(&taskBlock);
  arts::YieldOp::create(bodyBuilder, loc);

  source.erase();
  return success();
}


} // namespace mlir::carts::arts::boundary
