///==========================================================================///
/// File: DistributedLaunchConsistency.cpp
///
/// Reconciles ARTS EDT placement with distributed DB ownership.
///==========================================================================///

#define GEN_PASS_DEF_DISTRIBUTEDLAUNCHCONSISTENCY
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "carts/utils/Debug.h"

#include <optional>
#include <utility>

ARTS_DEBUG_SETUP(distributed_launch_consistency);

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

static Value getPartitionOwnerCoord(OpBuilder &builder, DbAllocOp alloc,
                                    DbAcquireOp acquire,
                                    const DbOwnerMapPlan &plan,
                                    unsigned ownerSlot) {
  ValueRange partitionOffsets = acquire.getPartitionOffsets();
  if (partitionOffsets.empty() || ownerSlot >= plan.blockShape.size())
    return {};

  Location loc = acquire.getLoc();
  if (auto planOwnerDims = readI64ArrayAttr(getPlanOwnerDimsAttr(alloc))) {
    if (ownerSlot < planOwnerDims->size()) {
      int64_t physicalDim = (*planOwnerDims)[ownerSlot];
      if (physicalDim >= 0 &&
          static_cast<size_t>(physicalDim) < partitionOffsets.size())
        return createBlockCoordFromElementOffset(builder, loc,
                                                 partitionOffsets[physicalDim],
                                                 plan.blockShape[ownerSlot]);
    }
  }

  if (ownerSlot < partitionOffsets.size())
    return createBlockCoordFromElementOffset(
        builder, loc, partitionOffsets[ownerSlot], plan.blockShape[ownerSlot]);
  return {};
}

static SmallVector<Value, 4>
getAcquireOwnerCoords(OpBuilder &builder, DbAllocOp alloc, DbAcquireOp acquire,
                      const DbOwnerMapPlan &plan, unsigned rank) {
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
    if (Value partitionCoord =
            getPartitionOwnerCoord(builder, alloc, acquire, plan, i)) {
      coords.push_back(partitionCoord);
      continue;
    }
    coords.push_back(createIndexConstant(builder, acquire.getLoc(), 0));
  }
  return coords;
}

struct OwnerCoordKey {
  Value value;
  bool implicitZero = false;
};

struct WriterOwnerTarget {
  DbAllocOp alloc;
  DbAcquireOp acquire;
  DbOwnerMapPlan plan;
  SmallVector<Value, 4> dbSizes;
  SmallVector<OwnerCoordKey, 4> coords;
};

static SmallVector<OwnerCoordKey, 4>
getAcquireOwnerCoordKeys(DbAcquireOp acquire, unsigned rank) {
  SmallVector<OwnerCoordKey, 4> coords;
  ValueRange offsets = acquire.getOffsets();
  ValueRange indices = acquire.getIndices();
  coords.reserve(rank);
  for (unsigned i = 0; i < rank; ++i) {
    if (i < offsets.size()) {
      coords.push_back({offsets[i], false});
      continue;
    }
    if (i < indices.size()) {
      coords.push_back({indices[i], false});
      continue;
    }
    coords.push_back({Value{}, true});
  }
  return coords;
}

static bool sameOwnerRoutePlan(const DbOwnerMapPlan &lhs,
                               const DbOwnerMapPlan &rhs) {
  if (lhs.kind != rhs.kind)
    return false;
  if (lhs.kind == DbOwnerMapKind::linear_mod_nodes)
    return true;
  return sameI64Values(lhs.dims, rhs.dims);
}

static bool equivalentIndexValues(Value lhs, Value rhs) {
  if (lhs == rhs)
    return true;
  if (!lhs || !rhs)
    return false;

  int64_t lhsConstant = 0;
  int64_t rhsConstant = 0;
  bool lhsIsConstant = ValueAnalysis::getConstantIndex(
      ValueAnalysis::stripNumericCasts(lhs), lhsConstant);
  bool rhsIsConstant = ValueAnalysis::getConstantIndex(
      ValueAnalysis::stripNumericCasts(rhs), rhsConstant);
  return lhsIsConstant && rhsIsConstant && lhsConstant == rhsConstant;
}

static bool equivalentOwnerCoord(OwnerCoordKey lhs, OwnerCoordKey rhs) {
  if (lhs.implicitZero && rhs.implicitZero)
    return true;

  auto isConstantZero = [](Value value) {
    int64_t constant = 0;
    return value &&
           ValueAnalysis::getConstantIndex(
               ValueAnalysis::stripNumericCasts(value), constant) &&
           constant == 0;
  };

  if (lhs.implicitZero)
    return isConstantZero(rhs.value);
  if (rhs.implicitZero)
    return isConstantZero(lhs.value);
  return equivalentIndexValues(lhs.value, rhs.value);
}

static SmallVector<unsigned, 4>
getRouteComparisonDims(const DbOwnerMapPlan &plan, unsigned dbRank) {
  SmallVector<unsigned, 4> dims;
  if (plan.kind == DbOwnerMapKind::linear_mod_nodes) {
    dims.reserve(dbRank);
    for (unsigned dim = 0; dim < dbRank; ++dim)
      dims.push_back(dim);
    return dims;
  }

  dims.reserve(plan.dims.size());
  for (int64_t rawDim : plan.dims) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) >= dbRank)
      return {};
    dims.push_back(static_cast<unsigned>(rawDim));
  }
  return dims;
}

static bool sameWriterOwnerTarget(const WriterOwnerTarget &lhs,
                                  const WriterOwnerTarget &rhs) {
  if (!sameOwnerRoutePlan(lhs.plan, rhs.plan))
    return false;
  if (lhs.dbSizes.size() != rhs.dbSizes.size() ||
      lhs.coords.size() != rhs.coords.size())
    return false;

  SmallVector<unsigned, 4> dims =
      getRouteComparisonDims(lhs.plan, lhs.dbSizes.size());
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
                              DbAcquireOp acquire, const DbOwnerMapPlan &plan) {
  SmallVector<Value, 4> dbSizes(alloc.getSizes().begin(),
                                alloc.getSizes().end());
  if (dbSizes.empty())
    return {};

  SmallVector<Value, 4> coords =
      getAcquireOwnerCoords(builder, alloc, acquire, plan, dbSizes.size());
  Value totalNodes =
      RuntimeQueryOp::create(builder, loc, RuntimeQueryKind::totalNodes)
          .getResult();
  return createDbOwnerRouteForCoords(builder, loc, dbSizes, coords, totalNodes,
                                     plan);
}

static std::optional<WriterOwnerTarget> getWriterOwnerTarget(Value dep) {
  Operation *underlying = DbUtils::getUnderlyingDb(dep);
  auto acquire = dyn_cast_or_null<DbAcquireOp>(underlying);
  if (!acquire || !DbUtils::isWriterMode(acquire.getMode()))
    return std::nullopt;

  auto alloc = dyn_cast_or_null<DbAllocOp>(DbUtils::getUnderlyingDbAlloc(dep));
  if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
    return std::nullopt;
  auto ownerMap = getDbOwnerMapPlan(alloc);
  if (!ownerMap)
    return std::nullopt;

  SmallVector<Value, 4> dbSizes(alloc.getSizes().begin(),
                                alloc.getSizes().end());
  if (dbSizes.empty())
    return std::nullopt;

  WriterOwnerTarget target;
  target.alloc = alloc;
  target.acquire = acquire;
  target.plan = *ownerMap;
  target.dbSizes = std::move(dbSizes);
  target.coords = getAcquireOwnerCoordKeys(acquire, target.dbSizes.size());
  return target;
}

struct DistributedLaunchConsistencyPass
    : public impl::DistributedLaunchConsistencyBase<
          DistributedLaunchConsistencyPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    unsigned localized = 0;
    unsigned routed = 0;

    module.walk([&](EdtOp edt) {
      if (edt.getConcurrency() != EdtConcurrency::internode)
        return;
      if (!DbUtils::hasLocalOnlyDistributedLaunchDependency(edt))
        return;

      OpBuilder builder(edt);
      Value localRoute = createCurrentNodeRoute(builder, edt.getLoc());
      edt.setConcurrency(EdtConcurrency::intranode);
      edt.getRouteMutable().set(localRoute);
      ++localized;
      ARTS_DEBUG(
          "Localized internode EDT with rejected distributed DB dep: " << edt);
    });

    module.walk([&](EdtOp edt) {
      if (edt.getConcurrency() != EdtConcurrency::internode)
        return;
      if (DbUtils::hasLocalOnlyDistributedLaunchDependency(edt))
        return;

      std::optional<WriterOwnerTarget> expectedOwner;
      bool sawDistributedWriter = false;
      bool conflict = false;
      for (Value dep : edt.getDependencies()) {
        std::optional<WriterOwnerTarget> owner = getWriterOwnerTarget(dep);
        if (!owner)
          continue;
        sawDistributedWriter = true;
        if (!expectedOwner) {
          expectedOwner = std::move(owner);
          continue;
        }
        if (!sameWriterOwnerTarget(*expectedOwner, *owner)) {
          conflict = true;
          break;
        }
      }

      if (!sawDistributedWriter || conflict || !expectedOwner)
        return;

      OpBuilder builder(edt);
      builder.setInsertionPoint(edt);
      Value expectedRoute =
          createOwnerRoute(builder, edt.getLoc(), expectedOwner->alloc,
                           expectedOwner->acquire, expectedOwner->plan);
      if (!expectedRoute)
        return;
      edt.getRouteMutable().set(expectedRoute);
      ++routed;
      ARTS_DEBUG("Routed internode EDT to distributed DB owner: " << edt);
    });

    ARTS_INFO("Distributed launch consistency localized "
              << localized << " EDTs and routed " << routed << " EDTs");
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createDistributedLaunchConsistencyPass() {
  return std::make_unique<DistributedLaunchConsistencyPass>();
}
