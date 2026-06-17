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
#include "carts/utils/DeadIrCleanup.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/STLExtras.h"

#include "carts/utils/Debug.h"

#include "llvm/ADT/DenseSet.h"
#include <limits>
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

static bool isMemrefDerivedFromDependency(Value memref, Value dependency) {
  memref = ValueAnalysis::stripMemrefViewOps(memref);
  dependency = ValueAnalysis::stripMemrefViewOps(dependency);
  if (!memref || !dependency)
    return false;
  if (ValueAnalysis::sameMemrefRoot(memref, dependency))
    return true;
  if (auto dbRef = memref.getDefiningOp<DbRefOp>())
    return isMemrefDerivedFromDependency(dbRef.getSource(), dependency);
  return false;
}

static bool edtWritesDependency(EdtOp edt, unsigned depIndex) {
  if (!edt || edt.getBody().empty() ||
      depIndex >= edt.getBody().front().getNumArguments())
    return false;
  Value depArg = edt.getBody().front().getArgument(depIndex);
  bool writes = false;
  edt.getBody().walk([&](Operation *op) {
    if (writes)
      return;
    std::optional<DbUtils::MemoryAccessInfo> access =
        DbUtils::getMemoryAccessInfo(op);
    if (!access || !access->isWrite())
      return;
    if (isMemrefDerivedFromDependency(access->memref, depArg))
      writes = true;
  });
  return writes;
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

static bool isKnownMultipleOf(Value value, int64_t divisor) {
  if (!value || divisor <= 0)
    return false;
  value = ValueAnalysis::stripNumericCasts(value);
  if (auto constant = ValueAnalysis::tryFoldConstantIndex(value))
    return *constant % divisor == 0;
  if (auto mul = value.getDefiningOp<arith::MulIOp>()) {
    std::optional<int64_t> lhs = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(mul.getLhs()));
    std::optional<int64_t> rhs = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(mul.getRhs()));
    return (lhs && *lhs % divisor == 0) || (rhs && *rhs % divisor == 0);
  }
  return false;
}

static bool isMulByConstant(Value value, Value base, int64_t multiplier) {
  if (!value || !base || multiplier <= 0)
    return false;
  value = ValueAnalysis::stripNumericCasts(value);
  base = ValueAnalysis::stripNumericCasts(base);
  if (multiplier == 1)
    return equivalentIndexValues(value, base);
  if (auto mul = value.getDefiningOp<arith::MulIOp>()) {
    std::optional<int64_t> lhs = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(mul.getLhs()));
    if (lhs && *lhs == multiplier && equivalentIndexValues(mul.getRhs(), base))
      return true;
    std::optional<int64_t> rhs = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(mul.getRhs()));
    if (rhs && *rhs == multiplier && equivalentIndexValues(mul.getLhs(), base))
      return true;
  }
  if (auto div = base.getDefiningOp<arith::DivUIOp>()) {
    std::optional<int64_t> rhs = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(div.getRhs()));
    if (rhs && *rhs == multiplier &&
        equivalentIndexValues(div.getLhs(), value) &&
        isKnownMultipleOf(value, multiplier))
      return true;
  }
  return false;
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
  if (lhs.divisor % rhs.divisor == 0 &&
      isMulByConstant(lhs.value, rhs.value, lhs.divisor / rhs.divisor))
    return true;
  if (rhs.divisor % lhs.divisor == 0 &&
      isMulByConstant(rhs.value, lhs.value, rhs.divisor / lhs.divisor))
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

struct WriterOwnerGroup {
  WriterOwnerTarget target;
  SmallVector<unsigned, 2> depIndices;
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

static std::optional<OwnerCoordKey>
getOwnerDimContiguousLeadingRouteKey(const WriterOwnerTarget &target,
                                     std::optional<int64_t> totalNodes) {
  if (!totalNodes || *totalNodes <= 0 ||
      target.facts.policy != DbOwnerRoutePolicy::OwnerDimContiguous ||
      target.facts.dims.empty() ||
      !ownerDimsAddressDbRank(target.facts.dims, target.dbSizes.size()))
    return std::nullopt;

  unsigned leadingDim = static_cast<unsigned>(target.facts.dims.front());
  if (leadingDim >= target.dbSizes.size() || leadingDim >= target.coords.size())
    return std::nullopt;

  std::optional<int64_t> leadingSize = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(target.dbSizes[leadingDim]));
  if (!leadingSize || *leadingSize <= 0 || *leadingSize < *totalNodes ||
      *leadingSize % *totalNodes != 0)
    return std::nullopt;

  int64_t routeChunk = *leadingSize / *totalNodes;
  OwnerCoordKey key = target.coords[leadingDim];
  if (key.implicitZero)
    return key;
  if (key.divisor <= 0 ||
      routeChunk > std::numeric_limits<int64_t>::max() / key.divisor)
    return std::nullopt;
  key.divisor *= routeChunk;
  return key;
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

static bool isKnownSingleBlockSpan(Value value) {
  if (!value)
    return true;
  std::optional<int64_t> upperBound = getKnownSpanUpperBound(value);
  return upperBound && *upperBound <= 1;
}

static std::optional<int64_t> getDispatchLoopStep(Value base,
                                                  Value normalizedLower = {}) {
  base = ValueAnalysis::stripNumericCasts(base);
  auto blockArg = dyn_cast<BlockArgument>(base);
  auto loop =
      blockArg
          ? dyn_cast_or_null<scf::ForOp>(blockArg.getOwner()->getParentOp())
          : scf::ForOp();
  if (!loop || loop.getInductionVar() != base)
    return std::nullopt;
  if (normalizedLower) {
    if (!equivalentIndexValues(normalizedLower, loop.getLowerBound()))
      return std::nullopt;
  } else if (!ValueAnalysis::isZeroConstant(loop.getLowerBound())) {
    return std::nullopt;
  }

  std::optional<int64_t> step =
      ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  if (!step || *step <= 0)
    return std::nullopt;
  return step;
}

static std::optional<std::pair<Value, int64_t>>
getMulValueAndConstant(Value value);

static Value stripZeroLowerClamp(Value value) {
  value = ValueAnalysis::stripNumericCasts(value);
  if (auto max = value.getDefiningOp<arith::MaxSIOp>()) {
    if (ValueAnalysis::isZeroConstant(max.getLhs()))
      return ValueAnalysis::stripNumericCasts(max.getRhs());
    if (ValueAnalysis::isZeroConstant(max.getRhs()))
      return ValueAnalysis::stripNumericCasts(max.getLhs());
  }
  if (auto max = value.getDefiningOp<arith::MaxUIOp>()) {
    if (ValueAnalysis::isZeroConstant(max.getLhs()))
      return ValueAnalysis::stripNumericCasts(max.getRhs());
    if (ValueAnalysis::isZeroConstant(max.getRhs()))
      return ValueAnalysis::stripNumericCasts(max.getLhs());
  }
  return value;
}

static std::optional<int64_t> getDivDispatchGroupBlockCount(arith::DivUIOp div,
                                                            int64_t scale = 1) {
  if (!div || scale <= 0)
    return std::nullopt;

  std::optional<int64_t> blockSize =
      ValueAnalysis::tryFoldConstantIndex(div.getRhs());
  if (!blockSize || *blockSize <= 0)
    return std::nullopt;

  Value lhs = stripZeroLowerClamp(div.getLhs());
  if (std::optional<std::pair<Value, int64_t>> scaled =
          getMulValueAndConstant(lhs)) {
    if (scaled->second % *blockSize == 0) {
      int64_t scale = scaled->second / *blockSize;
      if (scale > 0) {
        if (auto innerDiv = ValueAnalysis::stripNumericCasts(scaled->first)
                                .getDefiningOp<arith::DivUIOp>()) {
          return getDivDispatchGroupBlockCount(innerDiv, scale);
        }
      }
    }
  }

  Value base = lhs;
  Value normalizedLower;
  if (auto sub = base.getDefiningOp<arith::SubIOp>()) {
    base = sub.getLhs();
    normalizedLower = sub.getRhs();
  }

  std::optional<int64_t> step = getDispatchLoopStep(base, normalizedLower);
  if (!step || *step <= 0 || *step % *blockSize != 0)
    return std::nullopt;
  int64_t blockSteps = *step / *blockSize;
  if (blockSteps > std::numeric_limits<int64_t>::max() / scale)
    return std::nullopt;
  return blockSteps * scale;
}

static std::optional<std::pair<Value, int64_t>>
getMulValueAndConstant(Value value) {
  auto mul =
      ValueAnalysis::stripNumericCasts(value).getDefiningOp<arith::MulIOp>();
  if (!mul)
    return std::nullopt;

  std::optional<int64_t> lhs = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(mul.getLhs()));
  if (lhs && *lhs > 0)
    return std::make_pair(mul.getRhs(), *lhs);

  std::optional<int64_t> rhs = ValueAnalysis::tryFoldConstantIndex(
      ValueAnalysis::stripNumericCasts(mul.getRhs()));
  if (rhs && *rhs > 0)
    return std::make_pair(mul.getLhs(), *rhs);

  return std::nullopt;
}

static std::optional<int64_t>
getDispatchGroupBlockCountFromOffset(Value offset) {
  Value strippedOffset =
      stripZeroLowerClamp(ValueAnalysis::stripNumericCasts(offset));
  if (auto div = strippedOffset.getDefiningOp<arith::DivUIOp>())
    return getDivDispatchGroupBlockCount(div);

  if (std::optional<std::pair<Value, int64_t>> scaled =
          getMulValueAndConstant(strippedOffset))
    if (auto div = ValueAnalysis::stripNumericCasts(scaled->first)
                       .getDefiningOp<arith::DivUIOp>())
      return getDivDispatchGroupBlockCount(div, scaled->second);

  Value base = strippedOffset;
  Value normalizedLower;
  if (auto sub = base.getDefiningOp<arith::SubIOp>()) {
    base = sub.getLhs();
    normalizedLower = sub.getRhs();
  }
  return getDispatchLoopStep(base, normalizedLower);
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

  if (facts.policy == DbOwnerRoutePolicy::OwnerDimContiguous &&
      ownerDimsAddressDbRank(facts.dims, dbSizes->size()) &&
      !facts.dims.empty()) {
    unsigned leadingDim = static_cast<unsigned>(facts.dims.front());
    if (leadingDim < sizes.size() && leadingDim < dbSizes->size() &&
        isKnownSingleBlockSpan(sizes[leadingDim]) &&
        (*dbSizes)[leadingDim] >= *nodes &&
        (*dbSizes)[leadingDim] % *nodes == 0)
      return false;
  }

  std::optional<SmallVector<int64_t, 4>> groupCounts =
      getDispatchGroupBlockCounts(acquire, dbSizeValues.size());
  if (groupCounts && isStaticDbOwnerGroupedBlockScheduleRouteLocal(
                         *dbSizes, *groupCounts, *nodes, facts))
    return false;

  return true;
}

static bool sameWriterOwnerTarget(const WriterOwnerTarget &lhs,
                                  const WriterOwnerTarget &rhs,
                                  std::optional<int64_t> totalNodes) {
  bool sameRouteFacts = sameOwnerRouteFacts(lhs.facts, rhs.facts);
  if (sameRouteFacts && lhs.dbSizes.size() == rhs.dbSizes.size() &&
      lhs.coords.size() == rhs.coords.size()) {
    SmallVector<unsigned, 4> dims =
        getRouteComparisonDims(lhs.facts, lhs.dbSizes.size());
    if (!dims.empty() && llvm::all_of(dims, [&](unsigned dim) {
          return equivalentIndexValues(lhs.dbSizes[dim], rhs.dbSizes[dim]) &&
                 equivalentOwnerCoord(lhs.coords[dim], rhs.coords[dim]);
        }))
      return true;
  }

  if (lhs.facts.policy == DbOwnerRoutePolicy::OwnerDimContiguous &&
      rhs.facts.policy == DbOwnerRoutePolicy::OwnerDimContiguous) {
    std::optional<OwnerCoordKey> lhsKey =
        getOwnerDimContiguousLeadingRouteKey(lhs, totalNodes);
    std::optional<OwnerCoordKey> rhsKey =
        getOwnerDimContiguousLeadingRouteKey(rhs, totalNodes);
    return lhsKey && rhsKey && equivalentOwnerCoord(*lhsKey, *rhsKey);
  }
  return false;
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
  for (auto [depIndex, dep] : llvm::enumerate(edt.getDependencies())) {
    WriterOwnerTargetResult owner =
        getWriterOwnerTarget(dep, totalNodes, multiOwnerRange);
    if (owner.status == WriterOwnerTargetStatus::NoDistributedWriter)
      continue;
    if (!edtWritesDependency(edt, static_cast<unsigned>(depIndex)))
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
    if (!sameWriterOwnerTarget(*expectedOwner, *owner.target, totalNodes)) {
      conflict = true;
      break;
    }
  }
  return expectedOwner;
}

static FailureOr<SmallVector<WriterOwnerGroup, 2>>
collectWriterOwnerGroups(EdtOp edt, bool &sawDistributedWriter,
                         bool &multiOwnerRange, bool &unroutable,
                         std::optional<int64_t> totalNodes) {
  SmallVector<WriterOwnerGroup, 2> groups;
  sawDistributedWriter = false;
  multiOwnerRange = false;
  unroutable = false;

  for (auto [depIndex, dep] : llvm::enumerate(edt.getDependencies())) {
    WriterOwnerTargetResult owner =
        getWriterOwnerTarget(dep, totalNodes, multiOwnerRange);
    if (owner.status == WriterOwnerTargetStatus::NoDistributedWriter)
      continue;
    if (!edtWritesDependency(edt, static_cast<unsigned>(depIndex)))
      continue;
    sawDistributedWriter = true;
    if (owner.status == WriterOwnerTargetStatus::MultiOwnerRange ||
        multiOwnerRange)
      return groups;
    if (owner.status == WriterOwnerTargetStatus::Unroutable || !owner.target) {
      unroutable = true;
      return groups;
    }

    bool merged = false;
    for (WriterOwnerGroup &group : groups) {
      if (!sameWriterOwnerTarget(group.target, *owner.target, totalNodes))
        continue;
      group.depIndices.push_back(static_cast<unsigned>(depIndex));
      merged = true;
      break;
    }
    if (merged)
      continue;

    WriterOwnerGroup group;
    group.target = std::move(*owner.target);
    group.depIndices.push_back(static_cast<unsigned>(depIndex));
    groups.push_back(std::move(group));
  }
  return groups;
}

static std::optional<unsigned> getWrittenDependencyIndex(EdtOp edt,
                                                         Operation *op) {
  std::optional<DbUtils::MemoryAccessInfo> access =
      DbUtils::getMemoryAccessInfo(op);
  if (!access || !access->isWrite())
    return std::nullopt;

  Block &body = edt.getBody().front();
  unsigned depCount = edt.getDependencies().size();
  for (unsigned idx = 0; idx < depCount && idx < body.getNumArguments();
       ++idx) {
    if (isMemrefDerivedFromDependency(access->memref, body.getArgument(idx)))
      return idx;
  }
  return std::nullopt;
}

static void cloneEdtBody(EdtOp source, EdtOp dest) {
  Block &sourceBody = source.getBody().front();
  Block &destBody = dest.getBody().front();
  Location loc = source.getLoc();
  for (Value dep : dest.getDependencies())
    destBody.addArgument(dep.getType(), loc);
  for (Value param : dest.getParams())
    destBody.addArgument(param.getType(), loc);

  IRMapping mapper;
  for (auto [oldArg, newArg] :
       llvm::zip_equal(sourceBody.getArguments(), destBody.getArguments()))
    mapper.map(oldArg, newArg);

  OpBuilder builder(dest.getContext());
  builder.setInsertionPointToStart(&destBody);
  for (Operation &op : sourceBody.without_terminator())
    builder.clone(op, mapper);
  YieldOp::create(builder, loc);
}

static void copyEdtNonStructuralAttrs(EdtOp source, EdtOp dest) {
  for (NamedAttribute attr : source->getAttrs()) {
    StringRef name = attr.getName().getValue();
    if (name == "type" || name == "concurrency" ||
        name == "operand_segment_sizes")
      continue;
    dest->setAttr(attr.getName(), attr.getValue());
  }
}

static void pruneWritesOutsideGroup(EdtOp edt,
                                    const llvm::DenseSet<unsigned> &keepDeps) {
  SmallVector<Operation *, 8> toErase;
  edt.walk([&](Operation *op) {
    std::optional<unsigned> writtenDep = getWrittenDependencyIndex(edt, op);
    if (writtenDep && !keepDeps.contains(*writtenDep))
      toErase.push_back(op);
  });
  for (Operation *op : llvm::reverse(toErase))
    op->erase();
}

static void
removeUnusedEdtDependencies(EdtOp edt, SmallVectorImpl<DbAcquireOp> &acquires) {
  Block &body = edt.getBody().front();
  ValueRange deps = edt.getDependencies();
  if (body.getNumArguments() < deps.size())
    return;

  SmallVector<unsigned, 4> deadIndices;
  for (unsigned idx = 0; idx < deps.size(); ++idx)
    if (body.getArgument(idx).use_empty())
      deadIndices.push_back(idx);
  if (deadIndices.empty())
    return;

  llvm::sort(deadIndices, std::greater<>());
  deadIndices.erase(std::unique(deadIndices.begin(), deadIndices.end()),
                    deadIndices.end());
  llvm::DenseSet<unsigned> deadIndexSet(deadIndices.begin(), deadIndices.end());

  SmallVector<Value, 4> newDeps;
  for (unsigned idx = 0; idx < deps.size(); ++idx) {
    if (deadIndexSet.contains(idx)) {
      if (auto acquire = deps[idx].getDefiningOp<DbAcquireOp>())
        acquires.push_back(acquire);
      continue;
    }
    newDeps.push_back(deps[idx]);
  }

  for (unsigned idx : deadIndices)
    body.eraseArgument(idx);
  edt.setDependencies(newDeps);
}

static bool
splitConflictingDistributedWriterEdt(ModuleOp module, EdtOp edt,
                                     ArrayRef<WriterOwnerGroup> groups) {
  if (groups.size() <= 1)
    return false;

  SmallVector<DbAcquireOp, 8> acquireCleanupCandidates;
  SmallVector<EdtOp, 4> clones;
  OpBuilder builder(edt);
  for (const WriterOwnerGroup &group : groups) {
    builder.setInsertionPoint(edt);
    auto clone = EdtOp::create(builder, edt.getLoc(), edt.getType(),
                               edt.getConcurrency(), edt.getRoute(),
                               edt.getDependencies(), edt.getParams());
    copyEdtNonStructuralAttrs(edt, clone);
    cloneEdtBody(edt, clone);

    llvm::DenseSet<unsigned> keepDeps(group.depIndices.begin(),
                                      group.depIndices.end());
    pruneWritesOutsideGroup(clone, keepDeps);
    clones.push_back(clone);
  }

  edt.erase();

  bool changed = true;
  while (changed)
    changed = runDeadIrCleanup(module, /*removeSymbols=*/false).total() != 0;

  for (EdtOp clone : clones)
    removeUnusedEdtDependencies(clone, acquireCleanupCandidates);

  for (DbAcquireOp acquire : acquireCleanupCandidates) {
    if (!acquire)
      continue;
    Value guid = acquire.getGuid();
    Value ptr = acquire.getPtr();
    if ((!guid || guid.use_empty()) && (!ptr || ptr.use_empty()))
      acquire.erase();
  }
  return true;
}

struct WriterOwnerRoutePass
    : public impl::WriterOwnerRouteBase<WriterOwnerRoutePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    std::optional<int64_t> totalNodes = arts::getRuntimeTotalNodes(module);
    std::optional<int64_t> validationNodes = totalNodes;
    if (validationNodes && *validationNodes <= 1)
      validationNodes = 2;
    unsigned promoted = 0;
    unsigned routed = 0;
    unsigned split = 0;
    bool failed = false;

    SmallVector<EdtOp, 8> splitCandidates;
    module.walk([&](EdtOp edt) {
      if (edt.getType() != EdtType::task)
        return;
      if (DbUtils::hasLocalOnlyDistributedLaunchDependency(edt))
        return;

      bool sawDistributedWriter = false;
      bool multiOwnerRange = false;
      bool unroutable = false;
      FailureOr<SmallVector<WriterOwnerGroup, 2>> groups =
          collectWriterOwnerGroups(edt, sawDistributedWriter, multiOwnerRange,
                                   unroutable, validationNodes);
      if (::mlir::failed(groups) || multiOwnerRange || unroutable ||
          !sawDistributedWriter || groups->size() <= 1)
        return;
      splitCandidates.push_back(edt);
    });

    for (EdtOp edt : splitCandidates) {
      bool sawDistributedWriter = false;
      bool multiOwnerRange = false;
      bool unroutable = false;
      FailureOr<SmallVector<WriterOwnerGroup, 2>> groups =
          collectWriterOwnerGroups(edt, sawDistributedWriter, multiOwnerRange,
                                   unroutable, validationNodes);
      if (::mlir::failed(groups) || multiOwnerRange || unroutable ||
          !sawDistributedWriter || groups->size() <= 1)
        continue;
      if (splitConflictingDistributedWriterEdt(module, edt, *groups))
        ++split;
    }

    module.walk([&](EdtOp edt) {
      if (failed)
        return;
      if (edt.getType() != EdtType::task)
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
                                         validationNodes);

      if (multiOwnerRange) {
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

    ARTS_INFO("Writer owner route promoted "
              << promoted << " EDTs and routed " << routed << " EDTs; split "
              << split << " conflicting writer EDTs");
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createWriterOwnerRoutePass() {
  return std::make_unique<WriterOwnerRoutePass>();
}
