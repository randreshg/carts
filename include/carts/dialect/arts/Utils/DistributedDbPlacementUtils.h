#ifndef CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/PartitionPredicates.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>

namespace mlir::carts::arts {

enum class DbOwnerRoutePolicy {
  LinearModNodes,
  OwnerDimContiguous,
};

struct DbOwnerRouteFacts {
  DbOwnerRoutePolicy policy = DbOwnerRoutePolicy::LinearModNodes;
  SmallVector<int64_t, 4> dims;
  SmallVector<int64_t, 4> blockShape;
};

inline std::optional<uint64_t> getDistributedDbRuntimeInitBaseId(DbAllocOp op) {
  if (!op)
    return std::nullopt;
  if (int64_t artsId = getArtsId(op.getOperation()); artsId > 0)
    return static_cast<uint64_t>(artsId);
  if (auto createId =
          op->getAttrOfType<IntegerAttr>(AttrNames::Operation::ArtsCreateId)) {
    int64_t value = createId.getInt();
    if (value > 0)
      return static_cast<uint64_t>(value);
  }
  return std::nullopt;
}

inline std::optional<std::string>
getDistributedDbRuntimeInitBaseName(DbAllocOp op) {
  std::optional<uint64_t> baseId = getDistributedDbRuntimeInitBaseId(op);
  if (!baseId)
    return std::nullopt;
  return "__carts_dist_alloc_" + std::to_string(*baseId);
}

enum class DbOwnerRouteFailure {
  None,
  UnrealizableDbGrid,
  LocalOnlyConflict,
  OwnerDimsDoNotMatchDbRank,
  BlockShapeDoesNotMatchOwnerRank,
  OwnerDimsOutsideDbRank,
};

inline SmallVector<int64_t, 4> makeAllDbOwnerDims(unsigned rank) {
  SmallVector<int64_t, 4> dims;
  dims.reserve(rank);
  for (unsigned i = 0; i < rank; ++i)
    dims.push_back(static_cast<int64_t>(i));
  return dims;
}

inline SmallVector<int64_t, 4> makeLeadingDbOwnerDims(unsigned dbRank,
                                                      unsigned elementRank) {
  if (dbRank == 0 || elementRank == 0)
    return {};
  return makeAllDbOwnerDims(std::min(dbRank, elementRank));
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

/// Physical DB block layout recoverable from an ARTS DB allocation.
struct ArtsDbPhysicalLayout {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
};

struct ArtsOwnerSlotMapping {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<unsigned, 4> loopDims;
  SmallVector<int64_t, 4> blockSizes;
  SmallVector<unsigned, 4> rawSlots;
};

inline std::optional<ArtsDbPhysicalLayout>
readArtsDbPhysicalLayout(Operation *op) {
  if (!op)
    return std::nullopt;
  auto alloc = dyn_cast<DbAllocOp>(op);
  if (!alloc)
    return std::nullopt;
  auto partition = alloc.getPartitionMode();
  if (!partition || !usesBlockLayout(*partition) || alloc.getSizes().empty() ||
      alloc.getElementSizes().empty())
    return std::nullopt;
  ArtsDbPhysicalLayout layout;
  layout.ownerDims = makeLeadingDbOwnerDims(alloc.getSizes().size(),
                                            alloc.getElementSizes().size());
  if (layout.ownerDims.empty())
    return std::nullopt;
  for (Value elementSize : alloc.getElementSizes()) {
    std::optional<int64_t> constant = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(elementSize));
    if (!constant || *constant <= 0)
      return std::nullopt;
    layout.physicalBlockShape.push_back(*constant);
  }
  return layout;
}

inline bool hasArtsDbPhysicalLayout(Operation *op) {
  return readArtsDbPhysicalLayout(op).has_value();
}

inline FailureOr<ArtsOwnerSlotMapping>
resolveArtsOwnerSlotMapping(ArrayRef<int64_t> ownerDims,
                            ArrayRef<int64_t> blockShape, unsigned loopRank,
                            Operation *context) {
  if (ownerDims.empty()) {
    context->emitError()
        << "requires at least one committed physical owner dimension";
    return failure();
  }
  if (ownerDims.size() > loopRank) {
    context->emitError()
        << "names more physical owner dimensions than loop dimensions";
    return failure();
  }
  bool ownerRankLoop = loopRank == ownerDims.size();
  if (blockShape.size() != ownerDims.size() && blockShape.size() < loopRank) {
    context->emitError()
        << "requires physicalBlockShape rank to cover owner or loop rank";
    return failure();
  }

  SmallVector<char, 4> seenOwner;
  unsigned ownerSeenSize =
      std::max<unsigned>(loopRank, static_cast<unsigned>(blockShape.size()));
  seenOwner.assign(ownerSeenSize, 0);
  SmallVector<char, 4> seenLoop(loopRank, 0);
  SmallVector<std::tuple<int64_t, unsigned, int64_t, unsigned>, 4> slots;
  slots.reserve(ownerDims.size());
  for (auto [rawSlot, ownerDim] : llvm::enumerate(ownerDims)) {
    if (ownerDim < 0) {
      context->emitError() << "owner dim is negative";
      return failure();
    }
    unsigned physicalOwnerDim = static_cast<unsigned>(ownerDim);
    if (physicalOwnerDim >= seenOwner.size())
      seenOwner.resize(physicalOwnerDim + 1, 0);
    if (seenOwner[physicalOwnerDim]) {
      context->emitError() << "commits duplicate physical owner dimensions";
      return failure();
    }
    seenOwner[physicalOwnerDim] = 1;

    unsigned loopDim = physicalOwnerDim;
    if (static_cast<unsigned>(ownerDim) >= loopRank) {
      if (!ownerRankLoop) {
        context->emitError() << "owner dim exceeds loop rank";
        return failure();
      }
      loopDim = static_cast<unsigned>(rawSlot);
    }
    if (seenLoop[loopDim]) {
      context->emitError()
          << "maps multiple physical owner dimensions to one loop dimension";
      return failure();
    }
    seenLoop[loopDim] = 1;

    int64_t blockSize = 0;
    if (blockShape.size() == loopRank &&
        static_cast<size_t>(ownerDim) < blockShape.size()) {
      blockSize = blockShape[ownerDim];
    } else if (blockShape.size() == ownerDims.size()) {
      blockSize = blockShape[rawSlot];
    } else {
      context->emitError()
          << "physicalOwnerDims must index physicalBlockShape dimensions";
      return failure();
    }
    if (blockSize <= 0) {
      context->emitError()
          << "requires a positive physical block size for every owner dim";
      return failure();
    }
    slots.emplace_back(ownerDim, loopDim, blockSize,
                       static_cast<unsigned>(rawSlot));
  }

  llvm::sort(slots, [](const auto &lhs, const auto &rhs) {
    return std::get<0>(lhs) < std::get<0>(rhs);
  });

  ArtsOwnerSlotMapping mapping;
  mapping.ownerDims.reserve(slots.size());
  mapping.loopDims.reserve(slots.size());
  mapping.blockSizes.reserve(slots.size());
  mapping.rawSlots.reserve(slots.size());
  for (auto [ownerDim, loopDim, blockSize, rawSlot] : slots) {
    mapping.ownerDims.push_back(ownerDim);
    mapping.loopDims.push_back(loopDim);
    mapping.blockSizes.push_back(blockSize);
    mapping.rawSlots.push_back(rawSlot);
  }
  return mapping;
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
getDbOwnerRouteDimsFromDbGrid(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;
  unsigned dbRank = alloc.getSizes().size();
  SmallVector<int64_t, 4> dims =
      makeLeadingDbOwnerDims(dbRank, alloc.getElementSizes().size());
  if (dims.empty())
    return std::nullopt;
  return dims;
}

inline std::optional<SmallVector<int64_t, 4>>
getDbOwnerRouteBlockShapeFromDbGrid(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;
  auto dbOwnerDims = getDbOwnerRouteDimsFromDbGrid(alloc);
  if (!dbOwnerDims || dbOwnerDims->empty())
    return std::nullopt;
  SmallVector<int64_t, 4> blockShape;
  blockShape.reserve(dbOwnerDims->size());
  if (alloc.getElementSizes().size() < dbOwnerDims->size())
    return std::nullopt;
  for (unsigned slot = 0; slot < dbOwnerDims->size(); ++slot) {
    std::optional<int64_t> value = ValueAnalysis::tryFoldConstantIndex(
        ValueAnalysis::stripNumericCasts(alloc.getElementSizes()[slot]));
    if (!value || *value <= 0)
      return std::nullopt;
    blockShape.push_back(*value);
  }
  return blockShape;
}

inline std::optional<SmallVector<int64_t, 4>>
getDbOwnerRouteBlockShapeFromDbGrid(DbAllocOp alloc,
                                    ArrayRef<int64_t> ownerDims) {
  if (!alloc || ownerDims.empty())
    return std::nullopt;
  SmallVector<int64_t, 4> blockShape;
  blockShape.reserve(ownerDims.size());
  for (int64_t rawDim : ownerDims) {
    if (rawDim < 0 ||
        static_cast<unsigned>(rawDim) >= alloc.getElementSizes().size())
      return std::nullopt;
    std::optional<int64_t> value =
        ValueAnalysis::tryFoldConstantIndex(ValueAnalysis::stripNumericCasts(
            alloc.getElementSizes()[static_cast<unsigned>(rawDim)]));
    if (!value || *value <= 0)
      return std::nullopt;
    blockShape.push_back(*value);
  }
  return blockShape;
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

inline Value createOwnerRouteLinearIndex(OpBuilder &builder, Location loc,
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

inline Value createOwnerRouteTotalElements(OpBuilder &builder, Location loc,
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
createOwnerRouteCoordsFromLinearIndex(OpBuilder &builder, Location loc,
                                      ArrayRef<Value> sizes,
                                      Value linearIndex) {
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

    Value stride = createOwnerRouteTotalElements(
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
                                         const DbOwnerRouteFacts &facts) {
  Value totalNodesI32 = castToI32(builder, loc, totalNodes);
  if (!totalNodesI32)
    return {};

  if (facts.policy == DbOwnerRoutePolicy::LinearModNodes) {
    Value linear = createOwnerRouteLinearIndex(builder, loc, dbSizes, dbCoords);
    Value linearI32 = castToI32(builder, loc, linear);
    if (!linearI32)
      return {};
    return arith::RemUIOp::create(builder, loc, linearI32, totalNodesI32);
  }

  if (facts.policy != DbOwnerRoutePolicy::OwnerDimContiguous ||
      !ownerDimsAddressDbRank(facts.dims, dbSizes.size()) ||
      dbCoords.size() < dbSizes.size())
    return {};

  SmallVector<Value, 4> ownerSizes;
  SmallVector<Value, 4> ownerCoords;
  ownerSizes.reserve(facts.dims.size());
  ownerCoords.reserve(facts.dims.size());
  for (int64_t dim : facts.dims) {
    ownerSizes.push_back(dbSizes[dim]);
    ownerCoords.push_back(dbCoords[dim]);
  }

  Value ownerLinear =
      createOwnerRouteLinearIndex(builder, loc, ownerSizes, ownerCoords);
  Value ownerSpace = createOwnerRouteTotalElements(builder, loc, ownerSizes);
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
                                              const DbOwnerRouteFacts &facts) {
  if (facts.policy == DbOwnerRoutePolicy::LinearModNodes) {
    Value totalNodesI32 = castToI32(builder, loc, totalNodes);
    Value linearI32 = castToI32(builder, loc, linearIndex);
    if (!totalNodesI32 || !linearI32)
      return {};
    return arith::RemUIOp::create(builder, loc, linearI32, totalNodesI32);
  }

  SmallVector<Value, 4> dbCoords =
      createOwnerRouteCoordsFromLinearIndex(builder, loc, dbSizes, linearIndex);
  if (dbCoords.size() != dbSizes.size())
    return {};
  return createDbOwnerRouteForCoords(builder, loc, dbSizes, dbCoords,
                                     totalNodes, facts);
}

inline std::optional<SmallVector<int64_t, 4>>
foldStaticDbIndexValues(ArrayRef<Value> values, bool requirePositive = true) {
  SmallVector<int64_t, 4> folded;
  folded.reserve(values.size());
  for (Value value : values) {
    std::optional<int64_t> constant =
        ValueAnalysis::tryFoldConstantIndex(value);
    if (!constant || (requirePositive ? *constant <= 0 : *constant < 0))
      return std::nullopt;
    folded.push_back(*constant);
  }
  return folded;
}

inline std::optional<int64_t> checkedMul(int64_t lhs, int64_t rhs) {
  if (lhs < 0 || rhs < 0)
    return std::nullopt;
  if (lhs != 0 && rhs > std::numeric_limits<int64_t>::max() / lhs)
    return std::nullopt;
  return lhs * rhs;
}

inline std::optional<int64_t> staticProduct(ArrayRef<int64_t> values) {
  int64_t product = 1;
  for (int64_t value : values) {
    if (value <= 0)
      return std::nullopt;
    std::optional<int64_t> next = checkedMul(product, value);
    if (!next)
      return std::nullopt;
    product = *next;
  }
  return product;
}

inline std::optional<SmallVector<int64_t, 4>>
staticRowMajorStrides(ArrayRef<int64_t> sizes) {
  SmallVector<int64_t, 4> strides(sizes.size(), 1);
  int64_t suffix = 1;
  for (int64_t idx = static_cast<int64_t>(sizes.size()) - 1; idx >= 0; --idx) {
    strides[idx] = suffix;
    if (sizes[idx] <= 0)
      return std::nullopt;
    std::optional<int64_t> next = checkedMul(suffix, sizes[idx]);
    if (!next)
      return std::nullopt;
    suffix = *next;
  }
  return strides;
}

inline std::optional<int64_t> staticLinearIndex(ArrayRef<int64_t> sizes,
                                                ArrayRef<int64_t> coords) {
  if (sizes.size() != coords.size())
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> strides = staticRowMajorStrides(sizes);
  if (!strides)
    return std::nullopt;

  int64_t linear = 0;
  for (auto [size, coord, stride] : llvm::zip_equal(sizes, coords, *strides)) {
    if (coord < 0 || coord >= size)
      return std::nullopt;
    std::optional<int64_t> term = checkedMul(coord, stride);
    if (!term || linear > std::numeric_limits<int64_t>::max() - *term)
      return std::nullopt;
    linear += *term;
  }
  return linear;
}

inline std::optional<int64_t>
staticOwnerDimContiguousRoute(int64_t ownerLinear, int64_t ownerSpace,
                              int64_t totalNodes) {
  if (ownerLinear < 0 || ownerSpace <= 0 || totalNodes <= 0)
    return std::nullopt;
  std::optional<int64_t> scaled = checkedMul(ownerLinear, totalNodes);
  if (!scaled)
    return std::nullopt;
  return *scaled / ownerSpace;
}

/// Proves that a rectangular DB-block range maps to one runtime owner under the
/// derived owner route. This is a static legality proof for grouped writer
/// CUs/EDTs; callers still materialize the real grouped acquire/EDT shape.
inline bool
isStaticDbOwnerBlockRangeRouteLocal(ArrayRef<int64_t> dbSizes,
                                    ArrayRef<int64_t> offsets,
                                    ArrayRef<int64_t> sizes, int64_t totalNodes,
                                    const DbOwnerRouteFacts &facts) {
  if (totalNodes <= 1)
    return true;
  if (dbSizes.empty() || offsets.size() != dbSizes.size() ||
      sizes.size() != dbSizes.size())
    return false;
  for (auto [dbSize, offset, size] : llvm::zip_equal(dbSizes, offsets, sizes)) {
    if (dbSize <= 0 || offset < 0 || size <= 0 || offset >= dbSize ||
        size > dbSize - offset)
      return false;
  }

  if (facts.policy == DbOwnerRoutePolicy::OwnerDimContiguous) {
    if (!ownerDimsAddressDbRank(facts.dims, dbSizes.size()))
      return false;

    SmallVector<int64_t, 4> ownerSizes;
    SmallVector<int64_t, 4> firstCoords;
    SmallVector<int64_t, 4> lastCoords;
    ownerSizes.reserve(facts.dims.size());
    firstCoords.reserve(facts.dims.size());
    lastCoords.reserve(facts.dims.size());
    for (int64_t rawDim : facts.dims) {
      unsigned dim = static_cast<unsigned>(rawDim);
      ownerSizes.push_back(dbSizes[dim]);
      firstCoords.push_back(offsets[dim]);
      lastCoords.push_back(offsets[dim] + sizes[dim] - 1);
    }

    std::optional<int64_t> ownerSpace = staticProduct(ownerSizes);
    std::optional<int64_t> firstLinear =
        staticLinearIndex(ownerSizes, firstCoords);
    std::optional<int64_t> lastLinear =
        staticLinearIndex(ownerSizes, lastCoords);
    if (!ownerSpace || !firstLinear || !lastLinear)
      return false;

    std::optional<int64_t> firstRoute =
        staticOwnerDimContiguousRoute(*firstLinear, *ownerSpace, totalNodes);
    std::optional<int64_t> lastRoute =
        staticOwnerDimContiguousRoute(*lastLinear, *ownerSpace, totalNodes);
    return firstRoute && lastRoute && *firstRoute == *lastRoute;
  }

  if (facts.policy == DbOwnerRoutePolicy::LinearModNodes) {
    std::optional<SmallVector<int64_t, 4>> strides =
        staticRowMajorStrides(dbSizes);
    if (!strides)
      return false;
    for (auto [size, stride] : llvm::zip_equal(sizes, *strides)) {
      if (size > 1 && stride % totalNodes != 0)
        return false;
    }
    return true;
  }

  return false;
}

inline bool isStaticDbOwnerGroupedBlockScheduleRouteLocal(
    ArrayRef<int64_t> dbSizes, ArrayRef<int64_t> groupBlockCounts,
    int64_t totalNodes, const DbOwnerRouteFacts &facts) {
  if (totalNodes <= 1)
    return true;
  if (dbSizes.empty() || groupBlockCounts.size() != dbSizes.size())
    return false;
  for (auto [dbSize, groupCount] : llvm::zip_equal(dbSizes, groupBlockCounts))
    if (dbSize <= 0 || groupCount <= 0)
      return false;

  SmallVector<int64_t, 4> offsets(dbSizes.size(), 0);
  SmallVector<int64_t, 4> rangeSizes(dbSizes.size(), 1);

  std::function<bool(unsigned)> visit = [&](unsigned dim) -> bool {
    if (dim == dbSizes.size())
      return isStaticDbOwnerBlockRangeRouteLocal(dbSizes, offsets, rangeSizes,
                                                 totalNodes, facts);
    int64_t groupCount = groupBlockCounts[dim];
    for (int64_t offset = 0; offset < dbSizes[dim]; offset += groupCount) {
      offsets[dim] = offset;
      rangeSizes[dim] = std::min(groupCount, dbSizes[dim] - offset);
      if (!visit(dim + 1))
        return false;
    }
    return true;
  };

  return visit(0);
}

inline bool ownerRouteDimsMatchDbGrid(DbAllocOp alloc,
                                      const DbOwnerRouteFacts &facts) {
  if (!alloc || alloc.getSizes().empty())
    return false;
  return ownerDimsAddressDbRank(facts.dims, alloc.getSizes().size());
}

inline bool ownerRouteBlockShapeMatchesDbGrid(DbAllocOp alloc,
                                              const DbOwnerRouteFacts &facts) {
  if (!alloc || facts.blockShape.size() != facts.dims.size())
    return false;
  if (alloc.getElementSizes().size() < facts.dims.size())
    return false;
  for (auto [ownerSlot, rawDim] : llvm::enumerate(facts.dims)) {
    if (rawDim < 0 ||
        static_cast<unsigned>(rawDim) >= alloc.getElementSizes().size())
      return false;
    std::optional<int64_t> value =
        ValueAnalysis::tryFoldConstantIndex(ValueAnalysis::stripNumericCasts(
            alloc.getElementSizes()[static_cast<unsigned>(rawDim)]));
    if (!value || *value <= 0 || *value != facts.blockShape[ownerSlot])
      return false;
  }
  return true;
}

inline std::optional<DbOwnerRouteFacts>
deriveDbOwnerRouteFactsFromDbGrid(DbAllocOp alloc);

inline DbOwnerRouteFailure getDistributedDbOwnerRouteFailure(DbAllocOp alloc) {
  if (!alloc)
    return DbOwnerRouteFailure::UnrealizableDbGrid;

  auto facts = deriveDbOwnerRouteFactsFromDbGrid(alloc);
  if (!facts)
    return DbOwnerRouteFailure::UnrealizableDbGrid;

  if (alloc.getLocalOnly().value_or(false))
    return DbOwnerRouteFailure::LocalOnlyConflict;
  if (!ownerRouteDimsMatchDbGrid(alloc, *facts))
    return DbOwnerRouteFailure::OwnerDimsDoNotMatchDbRank;
  if (!ownerRouteBlockShapeMatchesDbGrid(alloc, *facts))
    return DbOwnerRouteFailure::BlockShapeDoesNotMatchOwnerRank;

  switch (facts->policy) {
  case DbOwnerRoutePolicy::LinearModNodes:
  case DbOwnerRoutePolicy::OwnerDimContiguous:
    if (!ownerDimsAddressDbRank(facts->dims, alloc.getSizes().size()))
      return DbOwnerRouteFailure::OwnerDimsOutsideDbRank;
    break;
  }

  return DbOwnerRouteFailure::None;
}

inline const char *toString(DbOwnerRouteFailure failure) {
  switch (failure) {
  case DbOwnerRouteFailure::None:
    return "valid";
  case DbOwnerRouteFailure::UnrealizableDbGrid:
    return "cannot derive owner route from DB block grid";
  case DbOwnerRouteFailure::LocalOnlyConflict:
    return "distributed/local_only conflict";
  case DbOwnerRouteFailure::OwnerDimsDoNotMatchDbRank:
    return "derived owner dimensions do not match the DB block grid";
  case DbOwnerRouteFailure::BlockShapeDoesNotMatchOwnerRank:
    return "derived owner block shape rank does not match owner dimensions";
  case DbOwnerRouteFailure::OwnerDimsOutsideDbRank:
    return "derived owner dimensions outside DB rank";
  }
  return "unknown owner-route failure";
}

inline DbOwnerRoutePolicy chooseDbOwnerRoutePolicy(DbAllocOp alloc) {
  if (auto kind = getEdtDistributionKind(alloc.getOperation());
      kind && *kind == EdtDistributionKind::block_cyclic)
    return DbOwnerRoutePolicy::LinearModNodes;
  return DbOwnerRoutePolicy::OwnerDimContiguous;
}

/// Project the DB block grid into owner-route facts without mutating IR.
inline std::optional<DbOwnerRouteFacts>
deriveDbOwnerRouteFactsFromDbGrid(DbAllocOp alloc, DbOwnerRoutePolicy policy) {
  if (!alloc)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> ownerDims =
      getDbOwnerRouteDimsFromDbGrid(alloc);
  if (!ownerDims || ownerDims->empty() ||
      !ownerDimsAddressDbRank(*ownerDims, alloc.getSizes().size()))
    return std::nullopt;
  switch (policy) {
  case DbOwnerRoutePolicy::LinearModNodes:
  case DbOwnerRoutePolicy::OwnerDimContiguous:
    break;
  }

  auto ownerBlockShape = getDbOwnerRouteBlockShapeFromDbGrid(alloc, *ownerDims);
  if (!ownerBlockShape || ownerBlockShape->empty())
    return std::nullopt;

  DbOwnerRouteFacts facts;
  facts.policy = policy;
  facts.dims.assign(ownerDims->begin(), ownerDims->end());
  facts.blockShape.assign(ownerBlockShape->begin(), ownerBlockShape->end());
  return facts;
}

inline std::optional<DbOwnerRouteFacts>
deriveDbOwnerRouteFactsFromDbGrid(DbAllocOp alloc) {
  if (!alloc)
    return std::nullopt;
  DbOwnerRoutePolicy policy = chooseDbOwnerRoutePolicy(alloc);
  return deriveDbOwnerRouteFactsFromDbGrid(alloc, policy);
}

inline bool canDeriveDbOwnerRouteFromGrid(DbAllocOp alloc,
                                          DbOwnerRoutePolicy policy) {
  if (!alloc)
    return false;

  std::optional<DbOwnerRouteFacts> facts =
      deriveDbOwnerRouteFactsFromDbGrid(alloc, policy);
  return facts.has_value();
}

inline bool canDeriveDbOwnerRouteFromGrid(DbAllocOp alloc) {
  return deriveDbOwnerRouteFactsFromDbGrid(alloc).has_value();
}

inline bool destDbOwnerRouteMatchesSourcePolicy(DbAllocOp source,
                                                DbAllocOp dest) {
  if (!source || !dest)
    return false;
  std::optional<DbOwnerRouteFacts> sourceFacts =
      deriveDbOwnerRouteFactsFromDbGrid(source);
  if (!sourceFacts)
    return false;
  return canDeriveDbOwnerRouteFromGrid(dest, sourceFacts->policy);
}

} // namespace mlir::carts::arts

#endif // CARTS_DIALECT_ARTS_UTILS_DISTRIBUTEDDBPLACEMENTUTILS_H
