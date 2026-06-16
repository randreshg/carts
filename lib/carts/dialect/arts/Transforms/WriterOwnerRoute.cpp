///==========================================================================///
/// File: WriterOwnerRoute.cpp
///
/// Route distributed-writer EDTs to their DB owner. Carved from the former
/// DistributedLaunchConsistency pass (T031): this half owns owner-route
/// derivation and writer promotion; the mixed-dep rejection/localization half
/// lives in EdtSplitForMixedDeps.
///==========================================================================///

#define GEN_PASS_DEF_WRITEROWNERROUTE
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"

#include "carts/utils/Debug.h"

#include <optional>
#include <utility>

ARTS_DEBUG_SETUP(writer_owner_route);

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static Value createIndexConstant(OpBuilder &builder, Location loc,
                                 int64_t value) {
  return arith::ConstantIndexOp::create(builder, loc, value);
}

static Value createBlockCoordFromElementOffset(OpBuilder &builder, Location loc,
                                               Value elementOffset,
                                               int64_t blockSize) {
  if (!elementOffset || blockSize <= 0)
    return {};
  Value blockSizeValue = createIndexConstant(builder, loc, blockSize);
  Value offset = castToIndex(builder, loc, elementOffset);
  if (!offset)
    return {};
  return arith::DivUIOp::create(builder, loc, offset, blockSizeValue);
}

static std::optional<unsigned>
getOwnerSlotForDbDim(const DbOwnerRouteFacts &facts, unsigned dbDim) {
  if (facts.policy == DbOwnerRoutePolicy::LinearModNodes) {
    if (dbDim < facts.blockShape.size())
      return dbDim;
    return std::nullopt;
  }

  for (auto [ownerSlot, rawDim] : llvm::enumerate(facts.dims)) {
    if (rawDim >= 0 && static_cast<unsigned>(rawDim) == dbDim &&
        ownerSlot < facts.blockShape.size())
      return static_cast<unsigned>(ownerSlot);
  }
  return std::nullopt;
}

static std::optional<unsigned>
getPartitionPhysicalDimForOwnerSlot(DbAllocOp alloc, unsigned ownerSlot,
                                    unsigned partitionRank) {
  if (ownerSlot < partitionRank)
    return ownerSlot;
  return std::nullopt;
}

struct OwnerCoordKey {
  Value value;
  int64_t divisor = 1;
  bool implicitZero = false;
};

static std::optional<OwnerCoordKey>
getPartitionOwnerCoordKeyForEntry(DbAllocOp alloc, DbAcquireOp acquire,
                                  const DbOwnerRouteFacts &facts,
                                  unsigned dbDim, size_t entryIdx) {
  std::optional<unsigned> ownerSlot = getOwnerSlotForDbDim(facts, dbDim);
  if (!ownerSlot || *ownerSlot >= facts.blockShape.size())
    return std::nullopt;

  SmallVector<Value> partitionOffsets =
      acquire.getPartitionOffsetsForEntry(entryIdx);
  if (partitionOffsets.empty())
    return std::nullopt;

  std::optional<unsigned> physicalDim = getPartitionPhysicalDimForOwnerSlot(
      alloc, *ownerSlot, partitionOffsets.size());
  if (!physicalDim)
    return std::nullopt;

  int64_t blockSize = facts.blockShape[*ownerSlot];
  if (blockSize <= 0)
    return std::nullopt;

  return OwnerCoordKey{partitionOffsets[*physicalDim], blockSize, false};
}

static bool equivalentIndexValues(Value lhs, Value rhs) {
  if (lhs == rhs)
    return true;
  if (!lhs || !rhs)
    return false;

  if (ValueAnalysis::areValuesEquivalent(lhs, rhs))
    return true;

  int64_t lhsConstant = 0;
  int64_t rhsConstant = 0;
  bool lhsIsConstant = ValueAnalysis::getConstantIndex(
      ValueAnalysis::stripNumericCasts(lhs), lhsConstant);
  bool rhsIsConstant = ValueAnalysis::getConstantIndex(
      ValueAnalysis::stripNumericCasts(rhs), rhsConstant);
  return lhsIsConstant && rhsIsConstant && lhsConstant == rhsConstant;
}

static std::optional<int64_t>
getNormalizedOwnerCoordConstant(OwnerCoordKey key) {
  if (key.implicitZero)
    return int64_t{0};
  if (!key.value || key.divisor <= 0)
    return std::nullopt;

  int64_t constant = 0;
  if (!ValueAnalysis::getConstantIndex(
          ValueAnalysis::stripNumericCasts(key.value), constant))
    return std::nullopt;
  if (constant < 0)
    return std::nullopt;
  return constant / key.divisor;
}

static bool equivalentOwnerCoord(OwnerCoordKey lhs, OwnerCoordKey rhs) {
  if (lhs.implicitZero || rhs.implicitZero) {
    std::optional<int64_t> lhsConstant = getNormalizedOwnerCoordConstant(lhs);
    std::optional<int64_t> rhsConstant = getNormalizedOwnerCoordConstant(rhs);
    return lhsConstant && rhsConstant && *lhsConstant == *rhsConstant;
  }

  if (lhs.divisor == rhs.divisor && equivalentIndexValues(lhs.value, rhs.value))
    return true;

  std::optional<int64_t> lhsConstant = getNormalizedOwnerCoordConstant(lhs);
  std::optional<int64_t> rhsConstant = getNormalizedOwnerCoordConstant(rhs);
  return lhsConstant && rhsConstant && *lhsConstant == *rhsConstant;
}

static std::optional<OwnerCoordKey>
getStablePartitionOwnerCoordKey(DbAllocOp alloc, DbAcquireOp acquire,
                                const DbOwnerRouteFacts &facts,
                                unsigned dbDim) {
  size_t entries = acquire.getNumPartitionEntries();
  if (entries == 0)
    return std::nullopt;

  std::optional<OwnerCoordKey> selected;
  for (size_t entryIdx = 0; entryIdx < entries; ++entryIdx) {
    std::optional<OwnerCoordKey> current = getPartitionOwnerCoordKeyForEntry(
        alloc, acquire, facts, dbDim, entryIdx);
    if (!current)
      return std::nullopt;
    if (!selected) {
      selected = *current;
      continue;
    }
    if (!equivalentOwnerCoord(*selected, *current))
      return std::nullopt;
  }
  return selected;
}

static SmallVector<Value, 4>
getAcquireOwnerCoords(OpBuilder &builder, DbAllocOp alloc, DbAcquireOp acquire,
                      const DbOwnerRouteFacts &facts, unsigned rank) {
  SmallVector<Value, 4> coords;
  ValueRange offsets = acquire.getOffsets();
  ValueRange indices = acquire.getIndices();
  coords.reserve(rank);
  for (unsigned i = 0; i < rank; ++i) {
    if (i < offsets.size()) {
      coords.push_back(offsets[i]);
      continue;
    }
    if (i < indices.size()) {
      coords.push_back(indices[i]);
      continue;
    }
    if (std::optional<OwnerCoordKey> partitionKey =
            getStablePartitionOwnerCoordKey(alloc, acquire, facts, i)) {
      coords.push_back(createBlockCoordFromElementOffset(
          builder, acquire.getLoc(), partitionKey->value,
          partitionKey->divisor));
      continue;
    }
    coords.push_back(createIndexConstant(builder, acquire.getLoc(), 0));
  }
  return coords;
}

struct WriterOwnerTarget {
  DbAllocOp alloc;
  DbAcquireOp acquire;
  DbOwnerRouteFacts facts;
  SmallVector<Value, 4> dbSizes;
  SmallVector<OwnerCoordKey, 4> coords;
};

enum class WriterOwnerTargetStatus {
  NoDistributedWriter,
  Ready,
  MultiOwnerRange,
  Unroutable
};

struct WriterOwnerTargetResult {
  WriterOwnerTargetStatus status = WriterOwnerTargetStatus::NoDistributedWriter;
  std::optional<WriterOwnerTarget> target;
};

static std::optional<SmallVector<OwnerCoordKey, 4>>
getAcquireOwnerCoordKeys(DbAllocOp alloc, DbAcquireOp acquire,
                         const DbOwnerRouteFacts &facts, unsigned rank) {
  SmallVector<OwnerCoordKey, 4> coords;
  ValueRange offsets = acquire.getOffsets();
  ValueRange indices = acquire.getIndices();
  coords.reserve(rank);
  for (unsigned i = 0; i < rank; ++i) {
    if (i < offsets.size()) {
      coords.push_back({offsets[i], 1, false});
      continue;
    }
    if (i < indices.size()) {
      coords.push_back({indices[i], 1, false});
      continue;
    }
    if (std::optional<OwnerCoordKey> partitionKey =
            getStablePartitionOwnerCoordKey(alloc, acquire, facts, i)) {
      coords.push_back(*partitionKey);
      continue;
    }
    if (acquire.getNumPartitionEntries() > 0 &&
        getOwnerSlotForDbDim(facts, i).has_value())
      return std::nullopt;
    coords.push_back({Value{}, 1, true});
  }
  return coords;
}

static bool sameOwnerRouteFacts(const DbOwnerRouteFacts &lhs,
                                const DbOwnerRouteFacts &rhs) {
  if (lhs.policy != rhs.policy)
    return false;
  if (lhs.policy == DbOwnerRoutePolicy::LinearModNodes)
    return true;
  return sameI64Values(lhs.dims, rhs.dims);
}

static SmallVector<unsigned, 4>
getRouteComparisonDims(const DbOwnerRouteFacts &facts, unsigned dbRank) {
  SmallVector<unsigned, 4> dims;
  if (facts.policy == DbOwnerRoutePolicy::LinearModNodes) {
    dims.reserve(dbRank);
    for (unsigned dim = 0; dim < dbRank; ++dim)
      dims.push_back(dim);
    return dims;
  }

  dims.reserve(facts.dims.size());
  for (int64_t rawDim : facts.dims) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) >= dbRank)
      return {};
    dims.push_back(static_cast<unsigned>(rawDim));
  }
  return dims;
}

static bool isKnownSingleBlockSpan(Value value) {
  if (!value)
    return true;
  if (auto constant = ValueAnalysis::tryFoldConstantIndex(value))
    return *constant <= 1;
  if (ValueAnalysis::isOneLikeValue(value))
    return true;
  if (auto min = value.getDefiningOp<arith::MinUIOp>())
    return ValueAnalysis::isOneLikeValue(min.getLhs()) ||
           ValueAnalysis::isOneLikeValue(min.getRhs());
  return false;
}

static std::optional<int64_t> getKnownSpanUpperBound(Value value) {
  if (!value)
    return std::nullopt;
  if (auto constant = ValueAnalysis::tryFoldConstantIndex(value))
    return *constant > 0 ? std::optional<int64_t>(*constant) : std::nullopt;
  if (ValueAnalysis::isOneLikeValue(value))
    return int64_t{1};
  if (auto min = value.getDefiningOp<arith::MinUIOp>()) {
    std::optional<int64_t> lhs = getKnownSpanUpperBound(min.getLhs());
    std::optional<int64_t> rhs = getKnownSpanUpperBound(min.getRhs());
    if (lhs && rhs)
      return std::min(*lhs, *rhs);
    if (lhs)
      return lhs;
    return rhs;
  }
  return std::nullopt;
}

static std::optional<int64_t>
getDispatchGroupBlockCountFromOffset(Value offset) {
  auto div =
      ValueAnalysis::stripNumericCasts(offset).getDefiningOp<arith::DivUIOp>();
  if (!div)
    return std::nullopt;

  std::optional<int64_t> blockSize =
      ValueAnalysis::tryFoldConstantIndex(div.getRhs());
  if (!blockSize || *blockSize <= 0)
    return std::nullopt;

  Value base = div.getLhs();
  Value normalizedLower;
  if (auto sub = base.getDefiningOp<arith::SubIOp>()) {
    base = sub.getLhs();
    normalizedLower = sub.getRhs();
  } else {
    auto blockArg = dyn_cast<BlockArgument>(base);
    auto loop =
        blockArg
            ? dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp())
            : scf::ForOp();
    if (!loop || loop.getInductionVar() != base ||
        !ValueAnalysis::isZeroConstant(loop.getLowerBound()))
      return std::nullopt;
  }

  auto blockArg = dyn_cast<BlockArgument>(base);
  auto loop =
      blockArg
          ? dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp())
          : scf::ForOp();
  if (!loop || loop.getInductionVar() != base)
    return std::nullopt;
  if (normalizedLower &&
      !equivalentIndexValues(normalizedLower, loop.getLowerBound()))
    return std::nullopt;

  std::optional<int64_t> step =
      ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!step || *step <= 0 || *step % *blockSize != 0)
    return std::nullopt;
  return *step / *blockSize;
}

static std::optional<SmallVector<int64_t, 4>>
getDispatchGroupBlockCounts(DbAcquireOp acquire, unsigned dbRank) {
  if (acquire.getOffsets().size() < dbRank ||
      acquire.getSizes().size() < dbRank)
    return std::nullopt;

  SmallVector<int64_t, 4> groupCounts;
  groupCounts.reserve(dbRank);
  for (unsigned dim = 0; dim < dbRank; ++dim) {
    std::optional<int64_t> groupCount =
        getDispatchGroupBlockCountFromOffset(acquire.getOffsets()[dim]);
    std::optional<int64_t> spanUpperBound =
        getKnownSpanUpperBound(acquire.getSizes()[dim]);
    if (!groupCount || *groupCount <= 0 || !spanUpperBound ||
        *spanUpperBound > *groupCount)
      return std::nullopt;
    groupCounts.push_back(*groupCount);
  }
  return groupCounts;
}

static bool writerAcquireMaySpanMultipleOwners(DbAcquireOp acquire,
                                               const DbOwnerRouteFacts &facts,
                                               ArrayRef<Value> dbSizeValues,
                                               std::optional<int64_t> nodes) {
  if (nodes && *nodes <= 1)
    return false;

  SmallVector<unsigned, 4> dims =
      getRouteComparisonDims(facts, dbSizeValues.size());
  if (dims.empty())
    return true;

  ValueRange sizes = acquire.getSizes();
  bool hasNonSingleSpan = false;
  for (unsigned dim : dims) {
    if (dim >= sizes.size())
      return true;
    if (!isKnownSingleBlockSpan(sizes[dim]))
      hasNonSingleSpan = true;
  }
  if (!hasNonSingleSpan)
    return false;
  if (!nodes)
    return true;

  SmallVector<Value, 4> offsets(acquire.getOffsets().begin(),
                                acquire.getOffsets().end());
  SmallVector<Value, 4> rangeSizes(sizes.begin(), sizes.end());
  std::optional<SmallVector<int64_t, 4>> dbSizes =
      foldStaticDbIndexValues(dbSizeValues);
  std::optional<SmallVector<int64_t, 4>> staticOffsets =
      foldStaticDbIndexValues(offsets, /*requirePositive=*/false);
  std::optional<SmallVector<int64_t, 4>> staticRangeSizes =
      foldStaticDbIndexValues(rangeSizes);
  if (!dbSizes)
    return true;
  if (staticOffsets && staticRangeSizes &&
      isStaticDbOwnerBlockRangeRouteLocal(*dbSizes, *staticOffsets,
                                          *staticRangeSizes, *nodes, facts))
    return false;

  std::optional<SmallVector<int64_t, 4>> groupCounts =
      getDispatchGroupBlockCounts(acquire, dbSizeValues.size());
  if (groupCounts && isStaticDbOwnerGroupedBlockScheduleRouteLocal(
                         *dbSizes, *groupCounts, *nodes, facts))
    return false;

  return true;
}

static bool sameWriterOwnerTarget(const WriterOwnerTarget &lhs,
                                  const WriterOwnerTarget &rhs) {
  if (!sameOwnerRouteFacts(lhs.facts, rhs.facts))
    return false;
  if (lhs.dbSizes.size() != rhs.dbSizes.size() ||
      lhs.coords.size() != rhs.coords.size())
    return false;

  SmallVector<unsigned, 4> dims =
      getRouteComparisonDims(lhs.facts, lhs.dbSizes.size());
  if (dims.empty())
    return false;

  for (unsigned dim : dims) {
    if (!equivalentIndexValues(lhs.dbSizes[dim], rhs.dbSizes[dim]))
      return false;
    if (!equivalentOwnerCoord(lhs.coords[dim], rhs.coords[dim]))
      return false;
  }

  return true;
}

static Value createOwnerRoute(OpBuilder &builder, Location loc, DbAllocOp alloc,
                              DbAcquireOp acquire,
                              const DbOwnerRouteFacts &facts) {
  SmallVector<Value, 4> dbSizes(alloc.getSizes().begin(),
                                alloc.getSizes().end());
  if (dbSizes.empty())
    return {};

  SmallVector<Value, 4> coords =
      getAcquireOwnerCoords(builder, alloc, acquire, facts, dbSizes.size());
  Value totalNodes =
      RuntimeQueryOp::create(builder, loc, RuntimeQueryKind::totalNodes)
          .getResult();
  return createDbOwnerRouteForCoords(builder, loc, dbSizes, coords, totalNodes,
                                     facts);
}

static WriterOwnerTargetResult
getWriterOwnerTarget(Value dep, std::optional<int64_t> totalNodes,
                     bool &multiOwnerRange) {
  Operation *underlying = DbUtils::getUnderlyingDb(dep);
  auto acquire = dyn_cast_or_null<DbAcquireOp>(underlying);
  if (!acquire || !DbUtils::isWriterMode(acquire.getMode()))
    return {};

  auto alloc = dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
  if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
    return {};
  auto ownerRoute = deriveDbOwnerRouteFactsFromDbGrid(alloc);
  if (!ownerRoute)
    return {WriterOwnerTargetStatus::Unroutable, std::nullopt};

  SmallVector<Value, 4> dbSizes(alloc.getSizes().begin(),
                                alloc.getSizes().end());
  if (dbSizes.empty())
    return {WriterOwnerTargetStatus::Unroutable, std::nullopt};
  if (writerAcquireMaySpanMultipleOwners(acquire, *ownerRoute, dbSizes,
                                         totalNodes)) {
    multiOwnerRange = true;
    return {WriterOwnerTargetStatus::MultiOwnerRange, std::nullopt};
  }

  WriterOwnerTarget target;
  target.alloc = alloc;
  target.acquire = acquire;
  target.facts = *ownerRoute;
  target.dbSizes = std::move(dbSizes);
  std::optional<SmallVector<OwnerCoordKey, 4>> coords =
      getAcquireOwnerCoordKeys(alloc, acquire, target.facts,
                               target.dbSizes.size());
  if (!coords)
    return {WriterOwnerTargetStatus::Unroutable, std::nullopt};
  target.coords = std::move(*coords);
  return {WriterOwnerTargetStatus::Ready, std::move(target)};
}

static std::optional<WriterOwnerTarget>
getConsistentWriterOwnerTarget(EdtOp edt, bool &sawDistributedWriter,
                               bool &conflict, bool &multiOwnerRange,
                               bool &unroutable,
                               std::optional<int64_t> totalNodes) {
  std::optional<WriterOwnerTarget> expectedOwner;
  sawDistributedWriter = false;
  conflict = false;
  multiOwnerRange = false;
  unroutable = false;
  for (Value dep : edt.getDependencies()) {
    WriterOwnerTargetResult owner =
        getWriterOwnerTarget(dep, totalNodes, multiOwnerRange);
    if (owner.status == WriterOwnerTargetStatus::NoDistributedWriter)
      continue;
    sawDistributedWriter = true;
    if (owner.status == WriterOwnerTargetStatus::MultiOwnerRange ||
        multiOwnerRange)
      break;
    if (owner.status == WriterOwnerTargetStatus::Unroutable) {
      unroutable = true;
      break;
    }
    if (!owner.target) {
      unroutable = true;
      break;
    }
    if (!expectedOwner) {
      expectedOwner = std::move(owner.target);
      continue;
    }
    if (!sameWriterOwnerTarget(*expectedOwner, *owner.target)) {
      conflict = true;
      break;
    }
  }
  return expectedOwner;
}

struct WriterOwnerRoutePass
    : public impl::WriterOwnerRouteBase<WriterOwnerRoutePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
    const bool singleNodeRuntime = totalNodes && *totalNodes <= 1;
    unsigned promoted = 0;
    unsigned routed = 0;
    unsigned deferredSingleNode = 0;
    bool failed = false;

    module.walk([&](EdtOp edt) {
      if (failed)
        return;
      if (DbUtils::hasLocalOnlyDistributedLaunchDependency(edt))
        return;

      bool sawDistributedWriter = false;
      bool conflict = false;
      bool multiOwnerRange = false;
      bool unroutable = false;
      std::optional<WriterOwnerTarget> expectedOwner =
          getConsistentWriterOwnerTarget(edt, sawDistributedWriter, conflict,
                                         multiOwnerRange, unroutable,
                                         totalNodes);

      if (multiOwnerRange) {
        if (singleNodeRuntime) {
          ++deferredSingleNode;
          return;
        }
        edt.emitError()
            << "writes a distributed DB range that may span multiple owners; "
               "SDE-to-ARTS must split writer codelets into owner-local "
               "block ranges before distributed launch";
        failed = true;
        return;
      }
      if (!sawDistributedWriter)
        return;
      if (unroutable || conflict || !expectedOwner) {
        if (singleNodeRuntime) {
          ++deferredSingleNode;
          return;
        }
        edt.emitError()
            << "writes distributed DBs whose owner route cannot be derived "
               "from the DB block grid; ARTS must split or "
               "explicitly sequence this codelet before distributed launch";
        failed = true;
        return;
      }

      OpBuilder builder(edt);
      builder.setInsertionPoint(edt);
      Value expectedRoute =
          createOwnerRoute(builder, edt.getLoc(), expectedOwner->alloc,
                           expectedOwner->acquire, expectedOwner->facts);
      if (!expectedRoute) {
        if (singleNodeRuntime) {
          ++deferredSingleNode;
          return;
        }
        edt.emitError()
            << "writes a distributed DB but ARTS could not derive an owner "
               "route from the DB block grid";
        failed = true;
        return;
      }
      if (edt.getConcurrency() != EdtConcurrency::internode) {
        edt.setConcurrency(EdtConcurrency::internode);
        ++promoted;
      }
      edt.getRouteMutable().set(expectedRoute);
      ++routed;
      ARTS_DEBUG("Routed distributed writer EDT to DB owner: " << edt);
    });

    if (failed) {
      signalPassFailure();
      return;
    }

    if (deferredSingleNode)
      ARTS_INFO("Writer owner route promoted " << promoted << " EDTs, routed "
                                               << routed << " EDTs, deferred "
                                               << deferredSingleNode
                                               << " single-node EDTs");
    else
      ARTS_INFO("Writer owner route promoted " << promoted << " EDTs and routed "
                                               << routed << " EDTs");
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createWriterOwnerRoutePass() {
  return std::make_unique<WriterOwnerRoutePass>();
}
