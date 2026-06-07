#ifndef CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <numeric>
#include <optional>

namespace mlir::carts::arts {

inline constexpr int32_t kDbOwnerMapVersion = 1;

struct DbOwnerMapPlan {
  DbOwnerMapKind kind = DbOwnerMapKind::linear_mod_nodes;
  SmallVector<int64_t, 4> dims;
  SmallVector<int64_t, 4> blockShape;
};

enum class DbOwnerMapContractFailure {
  None,
  MissingOwnerMapPlan,
  UnsupportedVersion,
  MissingPreservedPlan,
  LocalOnlyConflict,
  RejectReasonConflict,
  OwnerDimsDoNotPreservePlan,
  BlockShapeDoesNotPreservePlan,
  OwnerDimsOutsideDbRank,
  UnsupportedOwnerMapKind,
};

inline SmallVector<int64_t, 4> makeAllDbOwnerDims(unsigned rank) {
  SmallVector<int64_t, 4> dims;
  dims.reserve(rank);
  for (unsigned i = 0; i < rank; ++i)
    dims.push_back(static_cast<int64_t>(i));
  return dims;
}

inline bool ownerDimsAddressDbRank(ArrayRef<int64_t> dims, unsigned rank) {
  if (dims.empty())
    return false;
  for (int64_t dim : dims) {
    if (dim < 0 || static_cast<unsigned>(dim) >= rank)
      return false;
  }
  return true;
}

/// Physical DB layout seed plan committed by SDE/CODIR onto an op (db_alloc,
/// edt, or epoch): the owner dims plus the per-block physical shape. This is the
/// single reader for the seed-plan attrs; callers that previously re-read
/// planOwnerDims / planPhysicalBlockShape inline route through here.
struct ArtsDbPhysicalLayout {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
};

inline std::optional<ArtsDbPhysicalLayout>
readArtsDbPhysicalLayout(Operation *op) {
  if (!op)
    return std::nullopt;
  auto ownerDims = readI64ArrayAttr(getPlanOwnerDimsAttr(op));
  auto blockShape = readI64ArrayAttr(getPlanPhysicalBlockShapeAttr(op));
  if (!ownerDims || ownerDims->empty() || !blockShape || blockShape->empty())
    return std::nullopt;
  ArtsDbPhysicalLayout layout;
  layout.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  layout.physicalBlockShape.assign(blockShape->begin(), blockShape->end());
  return layout;
}

inline bool hasArtsDbPhysicalLayoutPlan(Operation *op) {
  return readArtsDbPhysicalLayout(op).has_value();
}

/// DB block home/placement is read and written through the generated
/// DbAllocOp accessors `getDbMemoryPlacement()` / `setDbMemoryPlacement(...)`.

inline std::optional<DbOwnerMapPlan> getDbOwnerMapPlan(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;
  auto kindAttr = alloc.getOwnerMapKindAttr();
  auto versionAttr = alloc.getOwnerMapVersionAttr();
  auto dims = readI64ArrayAttr(alloc.getOwnerMapDimsAttr());
  auto blockShape = readI64ArrayAttr(alloc.getOwnerBlockShapeAttr());
  if (!kindAttr || !versionAttr || !dims || !blockShape)
    return std::nullopt;

  DbOwnerMapPlan plan;
  plan.kind = kindAttr.getValue();
  plan.dims.assign(dims->begin(), dims->end());
  plan.blockShape.assign(blockShape->begin(), blockShape->end());
  return plan;
}

inline bool hasCompleteDbOwnerMapPlan(DbAllocOp alloc) {
  return getDbOwnerMapPlan(alloc).has_value();
}

inline bool sameI64Values(ArrayRef<int64_t> lhs, ArrayRef<int64_t> rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (auto [left, right] : llvm::zip_equal(lhs, rhs))
    if (left != right)
      return false;
  return true;
}

inline std::optional<SmallVector<int64_t, 4>>
getDbOwnerMapDimsFromPlan(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  auto planOwnerDims = readI64ArrayAttr(getPlanOwnerDimsAttr(alloc));
  if (!planOwnerDims || planOwnerDims->empty())
    return std::nullopt;

  unsigned dbRank = alloc.getSizes().size();
  if (dbRank == 0)
    return std::nullopt;

  /// Physical owner dims are projected into the runtime DB coordinate space.
  /// When ARTS has lowered an N-D physical owner space to an N-D DB space, DB
  /// dim i corresponds to owner slot i, not necessarily physical dim i. This
  /// is what lets a non-leading physical owner dim such as [1] become DB-space
  /// owner_map_dims [0] for a rank-1 runtime DB.
  if (planOwnerDims->size() == dbRank) {
    unsigned physicalRank = alloc.getElementSizes().size();
    if (!ownerDimsAddressDbRank(*planOwnerDims, dbRank) &&
        !ownerDimsAddressDbRank(*planOwnerDims, physicalRank))
      return std::nullopt;
    return makeAllDbOwnerDims(dbRank);
  }

  /// Some hand-authored ARTS tests and materialized DBs already describe a
  /// higher-rank DB coordinate space directly. Keep accepting those when the
  /// preserved plan dims are valid DB-space dims.
  if (!ownerDimsAddressDbRank(*planOwnerDims, dbRank))
    return std::nullopt;

  SmallVector<int64_t, 4> dims;
  dims.assign(planOwnerDims->begin(), planOwnerDims->end());
  return dims;
}

inline std::optional<SmallVector<int64_t, 4>>
getDbOwnerBlockShapeFromPlan(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;

  auto planOwnerDims = readI64ArrayAttr(getPlanOwnerDimsAttr(alloc));
  auto planBlockShape = readI64ArrayAttr(getPlanPhysicalBlockShapeAttr(alloc));
  auto dbOwnerDims = getDbOwnerMapDimsFromPlan(alloc);
  if (!planOwnerDims || planOwnerDims->empty() || !planBlockShape ||
      planBlockShape->empty() || !dbOwnerDims || dbOwnerDims->empty())
    return std::nullopt;

  /// Full physical block shape: project by the physical owner dims and retain
  /// owner-slot order. For planOwnerDims [1] and planPhysicalBlockShape [8,
  /// 16], the rank-1 DB owner block shape is [16].
  if (planBlockShape->size() == alloc.getElementSizes().size()) {
    SmallVector<int64_t, 4> identityOwnerDims =
        makeAllDbOwnerDims(dbOwnerDims->size());
    if (planBlockShape->size() == dbOwnerDims->size() &&
        sameI64Values(*planOwnerDims, identityOwnerDims)) {
      SmallVector<int64_t, 4> blockShape;
      blockShape.assign(planBlockShape->begin(), planBlockShape->end());
      return blockShape;
    }

    SmallVector<int64_t, 4> blockShape;
    blockShape.reserve(planOwnerDims->size());
    for (int64_t physicalDim : *planOwnerDims) {
      if (physicalDim < 0 ||
          static_cast<size_t>(physicalDim) >= planBlockShape->size())
        return std::nullopt;
      blockShape.push_back((*planBlockShape)[physicalDim]);
    }
    return blockShape;
  }

  /// Already compacted to one block size per owner-map dimension.
  if (planBlockShape->size() == dbOwnerDims->size()) {
    SmallVector<int64_t, 4> blockShape;
    blockShape.assign(planBlockShape->begin(), planBlockShape->end());
    return blockShape;
  }

  /// DB-rank block shape: project by DB-space owner-map dims.
  if (planBlockShape->size() == alloc.getSizes().size()) {
    SmallVector<int64_t, 4> blockShape;
    blockShape.reserve(dbOwnerDims->size());
    for (int64_t dbDim : *dbOwnerDims) {
      if (dbDim < 0 || static_cast<size_t>(dbDim) >= planBlockShape->size())
        return std::nullopt;
      blockShape.push_back((*planBlockShape)[dbDim]);
    }
    return blockShape;
  }

  return std::nullopt;
}

inline Value createIndexConstant(OpBuilder &builder, Location loc,
                                 int64_t value) {
  return arith::ConstantIndexOp::create(builder, loc, value);
}

inline Value castToIndex(OpBuilder &builder, Location loc, Value value) {
  if (!value)
    return {};
  if (value.getType().isIndex())
    return value;
  if (isa<IntegerType>(value.getType()))
    return arith::IndexCastOp::create(builder, loc, builder.getIndexType(),
                                      value);
  return {};
}

inline Value castToI32(OpBuilder &builder, Location loc, Value value) {
  if (!value)
    return {};
  Type i32Type = builder.getI32Type();
  if (value.getType() == i32Type)
    return value;
  if (value.getType().isIndex())
    return arith::IndexCastOp::create(builder, loc, i32Type, value);
  if (auto integerType = dyn_cast<IntegerType>(value.getType())) {
    if (integerType.getWidth() < 32)
      return arith::ExtUIOp::create(builder, loc, i32Type, value);
    if (integerType.getWidth() > 32)
      return arith::TruncIOp::create(builder, loc, i32Type, value);
  }
  return {};
}

inline Value createOwnerMapLinearIndex(OpBuilder &builder, Location loc,
                                       ArrayRef<Value> sizes,
                                       ArrayRef<Value> indices) {
  if (indices.empty())
    return createIndexConstant(builder, loc, 0);
  if (sizes.size() < indices.size())
    return {};

  Value linear = castToIndex(builder, loc, indices.front());
  if (!linear)
    return {};
  for (size_t i = 1; i < indices.size(); ++i) {
    Value size = castToIndex(builder, loc, sizes[i]);
    Value index = castToIndex(builder, loc, indices[i]);
    if (!size || !index)
      return {};
    linear = arith::MulIOp::create(builder, loc, linear, size);
    linear = arith::AddIOp::create(builder, loc, linear, index);
  }
  return linear;
}

inline Value createOwnerMapTotalElements(OpBuilder &builder, Location loc,
                                         ArrayRef<Value> sizes) {
  Value total = createIndexConstant(builder, loc, 1);
  for (Value sizeValue : sizes) {
    Value size = castToIndex(builder, loc, sizeValue);
    if (!size)
      return {};
    total = arith::MulIOp::create(builder, loc, total, size);
  }
  return total;
}

inline SmallVector<Value, 4>
createOwnerMapCoordsFromLinearIndex(OpBuilder &builder, Location loc,
                                    ArrayRef<Value> sizes, Value linearIndex) {
  SmallVector<Value, 4> coords;
  if (sizes.empty())
    return coords;

  Value remaining = castToIndex(builder, loc, linearIndex);
  if (!remaining)
    return {};
  coords.reserve(sizes.size());

  for (size_t i = 0; i < sizes.size(); ++i) {
    if (i + 1 == sizes.size()) {
      coords.push_back(remaining);
      break;
    }

    Value stride = createOwnerMapTotalElements(
        builder, loc, ArrayRef<Value>(sizes).drop_front(i + 1));
    if (!stride)
      return {};
    Value coord = arith::DivUIOp::create(builder, loc, remaining, stride);
    coords.push_back(coord);
    remaining = arith::RemUIOp::create(builder, loc, remaining, stride);
  }

  return coords;
}

inline Value createDbOwnerRouteForCoords(OpBuilder &builder, Location loc,
                                         ArrayRef<Value> dbSizes,
                                         ArrayRef<Value> dbCoords,
                                         Value totalNodes,
                                         const DbOwnerMapPlan &plan) {
  Value totalNodesI32 = castToI32(builder, loc, totalNodes);
  if (!totalNodesI32)
    return {};

  if (plan.kind == DbOwnerMapKind::linear_mod_nodes) {
    Value linear = createOwnerMapLinearIndex(builder, loc, dbSizes, dbCoords);
    Value linearI32 = castToI32(builder, loc, linear);
    if (!linearI32)
      return {};
    return arith::RemUIOp::create(builder, loc, linearI32, totalNodesI32);
  }

  if (plan.kind != DbOwnerMapKind::owner_dim_contiguous ||
      !ownerDimsAddressDbRank(plan.dims, dbSizes.size()) ||
      dbCoords.size() < dbSizes.size())
    return {};

  SmallVector<Value, 4> ownerSizes;
  SmallVector<Value, 4> ownerCoords;
  ownerSizes.reserve(plan.dims.size());
  ownerCoords.reserve(plan.dims.size());
  for (int64_t dim : plan.dims) {
    ownerSizes.push_back(dbSizes[dim]);
    ownerCoords.push_back(dbCoords[dim]);
  }

  Value ownerLinear =
      createOwnerMapLinearIndex(builder, loc, ownerSizes, ownerCoords);
  Value ownerSpace = createOwnerMapTotalElements(builder, loc, ownerSizes);
  Value totalNodesIndex = castToIndex(builder, loc, totalNodesI32);
  if (!ownerLinear || !ownerSpace || !totalNodesIndex)
    return {};
  Value scaled =
      arith::MulIOp::create(builder, loc, ownerLinear, totalNodesIndex);
  Value routeIndex = arith::DivUIOp::create(builder, loc, scaled, ownerSpace);
  return castToI32(builder, loc, routeIndex);
}

inline Value createDbOwnerRouteForLinearIndex(OpBuilder &builder, Location loc,
                                              ArrayRef<Value> dbSizes,
                                              Value linearIndex,
                                              Value totalNodes,
                                              const DbOwnerMapPlan &plan) {
  if (plan.kind == DbOwnerMapKind::linear_mod_nodes) {
    Value totalNodesI32 = castToI32(builder, loc, totalNodes);
    Value linearI32 = castToI32(builder, loc, linearIndex);
    if (!totalNodesI32 || !linearI32)
      return {};
    return arith::RemUIOp::create(builder, loc, linearI32, totalNodesI32);
  }

  SmallVector<Value, 4> dbCoords =
      createOwnerMapCoordsFromLinearIndex(builder, loc, dbSizes, linearIndex);
  if (dbCoords.size() != dbSizes.size())
    return {};
  return createDbOwnerRouteForCoords(builder, loc, dbSizes, dbCoords,
                                     totalNodes, plan);
}

inline bool ownerMapPreservesPlanOwnerDims(DbAllocOp alloc,
                                           const DbOwnerMapPlan &plan) {
  auto dbOwnerDims = getDbOwnerMapDimsFromPlan(alloc);
  if (!dbOwnerDims)
    return false;
  if (plan.kind == DbOwnerMapKind::linear_mod_nodes)
    return ownerDimsAddressDbRank(plan.dims, alloc.getSizes().size());
  return sameI64Values(plan.dims, *dbOwnerDims);
}

inline bool ownerMapPreservesPlanBlockShape(DbAllocOp alloc,
                                            const DbOwnerMapPlan &plan) {
  auto dbOwnerBlockShape = getDbOwnerBlockShapeFromPlan(alloc);
  return dbOwnerBlockShape &&
         sameI64Values(plan.blockShape, *dbOwnerBlockShape);
}

inline DbOwnerMapContractFailure
getDistributedDbOwnerMapContractFailure(DbAllocOp alloc) {
  if (!alloc)
    return DbOwnerMapContractFailure::MissingOwnerMapPlan;

  auto plan = getDbOwnerMapPlan(alloc);
  if (!plan)
    return DbOwnerMapContractFailure::MissingOwnerMapPlan;

  if (!alloc.getOwnerMapVersionAttr() ||
      alloc.getOwnerMapVersionAttr().getInt() != kDbOwnerMapVersion)
    return DbOwnerMapContractFailure::UnsupportedVersion;

  if (!getPlanOwnerDimsAttr(alloc.getOperation()) ||
      !getPlanPhysicalBlockShapeAttr(alloc.getOperation()))
    return DbOwnerMapContractFailure::MissingPreservedPlan;

  if (alloc.getLocalOnly().value_or(false))
    return DbOwnerMapContractFailure::LocalOnlyConflict;
  if (alloc.getDistributedRejectReasonAttr())
    return DbOwnerMapContractFailure::RejectReasonConflict;

  if (!ownerMapPreservesPlanOwnerDims(alloc, *plan))
    return DbOwnerMapContractFailure::OwnerDimsDoNotPreservePlan;
  if (!ownerMapPreservesPlanBlockShape(alloc, *plan))
    return DbOwnerMapContractFailure::BlockShapeDoesNotPreservePlan;

  switch (plan->kind) {
  case DbOwnerMapKind::linear_mod_nodes:
  case DbOwnerMapKind::owner_dim_contiguous:
    if (!ownerDimsAddressDbRank(plan->dims, alloc.getSizes().size()))
      return DbOwnerMapContractFailure::OwnerDimsOutsideDbRank;
    break;
  case DbOwnerMapKind::owner_dim_grid:
  case DbOwnerMapKind::explicit_rank_table:
    return DbOwnerMapContractFailure::UnsupportedOwnerMapKind;
  }

  return DbOwnerMapContractFailure::None;
}

inline const char *toString(DbOwnerMapContractFailure failure) {
  switch (failure) {
  case DbOwnerMapContractFailure::None:
    return "valid";
  case DbOwnerMapContractFailure::MissingOwnerMapPlan:
    return "missing owner-map plan";
  case DbOwnerMapContractFailure::UnsupportedVersion:
    return "unsupported owner-map version";
  case DbOwnerMapContractFailure::MissingPreservedPlan:
    return "missing preserved owner dims or physical block shape";
  case DbOwnerMapContractFailure::LocalOnlyConflict:
    return "distributed/local_only conflict";
  case DbOwnerMapContractFailure::RejectReasonConflict:
    return "distributed reject-reason conflict";
  case DbOwnerMapContractFailure::OwnerDimsDoNotPreservePlan:
    return "owner_map_dims do not preserve planOwnerDims";
  case DbOwnerMapContractFailure::BlockShapeDoesNotPreservePlan:
    return "owner_block_shape does not preserve planPhysicalBlockShape";
  case DbOwnerMapContractFailure::OwnerDimsOutsideDbRank:
    return "owner_map_dims outside DB rank";
  case DbOwnerMapContractFailure::UnsupportedOwnerMapKind:
    return "unsupported owner-map kind";
  }
  return "unknown owner-map contract failure";
}

/// Precise reason a not-yet-realized owner-map kind fails closed. ARTS realizes
/// owner maps only from committed facts; it does not invent a process grid or a
/// per-block rank table when none is committed.
inline const char *ownerMapKindUnrealizableReason(DbOwnerMapKind kind) {
  switch (kind) {
  case DbOwnerMapKind::owner_dim_grid:
    return "uses owner_map_kind owner_dim_grid but carries no committed "
           "process-grid shape to realize a grid route; ARTS does not invent "
           "one";
  case DbOwnerMapKind::explicit_rank_table:
    return "uses owner_map_kind explicit_rank_table but carries no committed "
           "per-block rank table to realize a table route; ARTS does not "
           "invent one";
  case DbOwnerMapKind::linear_mod_nodes:
  case DbOwnerMapKind::owner_dim_contiguous:
    return "owner-map kind is realizable";
  }
  return "unknown owner-map kind";
}

inline DbOwnerMapKind chooseDbOwnerMapKind(DbAllocOp alloc) {
  if (auto kind = getEdtDistributionKind(alloc.getOperation());
      kind && *kind == EdtDistributionKind::block_cyclic)
    return DbOwnerMapKind::linear_mod_nodes;
  return DbOwnerMapKind::owner_dim_contiguous;
}

/// Realize the ARTS owner-map and scattered home of a distributed DB from the
/// committed SDE/CODIR seed plan. This is mechanical: owner dims and block shape
/// are projected from the plan, never recomputed. Returns false (caller fails
/// closed) when the plan cannot be projected into a usable owner map.
inline bool realizeDbOwnerMapFromPlan(DbAllocOp alloc) {
  if (!alloc)
    return false;

  auto blockShape = readI64ArrayAttr(getPlanPhysicalBlockShapeAttr(alloc));
  if (!blockShape || blockShape->empty())
    return false;

  auto ownerBlockShape = getDbOwnerBlockShapeFromPlan(alloc);
  if (!ownerBlockShape || ownerBlockShape->empty())
    return false;

  SmallVector<int64_t, 4> ownerMapDims;
  DbOwnerMapKind kind = chooseDbOwnerMapKind(alloc);
  if (kind == DbOwnerMapKind::linear_mod_nodes) {
    ownerMapDims = makeAllDbOwnerDims(alloc.getSizes().size());
  } else {
    auto dbOwnerDims = getDbOwnerMapDimsFromPlan(alloc);
    if (!dbOwnerDims ||
        !ownerDimsAddressDbRank(*dbOwnerDims, alloc.getSizes().size()))
      return false;
    ownerMapDims.assign(dbOwnerDims->begin(), dbOwnerDims->end());
  }

  MLIRContext *ctx = alloc.getContext();
  alloc.setOwnerMapKindAttr(DbOwnerMapKindAttr::get(ctx, kind));
  alloc.setOwnerMapVersionAttr(
      IntegerAttr::get(IntegerType::get(ctx, 32), kDbOwnerMapVersion));
  alloc.setOwnerMapDimsAttr(buildI64ArrayAttr(ctx, ownerMapDims));
  alloc.setOwnerBlockShapeAttr(buildI64ArrayAttr(ctx, *ownerBlockShape));
  /// A realized owner map means the blocks are scattered across owner ranks;
  /// record that home alongside the map so it is readable, not implicit.
  alloc.setDbMemoryPlacement(DbMemoryPlacement::owner_scattered);
  return true;
}

inline void copyDbOwnerMapAttrs(DbAllocOp source, DbAllocOp dest) {
  if (!source || !dest)
    return;
  if (auto attr = source.getOwnerMapKindAttr())
    dest.setOwnerMapKindAttr(attr);
  if (auto attr = source.getOwnerMapVersionAttr())
    dest.setOwnerMapVersionAttr(attr);
  if (auto attr = source.getOwnerMapDimsAttr())
    dest.setOwnerMapDimsAttr(attr);
  if (auto attr = source.getOwnerBlockShapeAttr())
    dest.setOwnerBlockShapeAttr(attr);
}

} // namespace mlir::carts::arts

#endif // CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H
