///==========================================================================///
/// File: DbLayoutFactsUtils.cpp
///==========================================================================///

#include "carts/dialect/arts/Utils/DbLayoutFactsUtils.h"

#include "carts/dialect/arts/Utils/PartitionPredicates.h"
#include "carts/dialect/arts/Utils/ValueAnalysisUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include <algorithm>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static Value ceilDivPositiveIndex(OpBuilder &builder, Location loc, Value value,
                                  Value divisor) {
  Value one = createOneIndex(builder, loc);
  divisor = arith::MaxUIOp::create(builder, loc, divisor, one);
  Value adjusted = arith::AddIOp::create(
      builder, loc, value, arith::SubIOp::create(builder, loc, divisor, one));
  return arith::DivUIOp::create(builder, loc, adjusted, divisor);
}

static SmallVector<Value, 4>
materializeIndexConstants(OpBuilder &builder, Location loc,
                          ArrayRef<int64_t> values) {
  SmallVector<Value, 4> result;
  result.reserve(values.size());
  for (int64_t value : values)
    result.push_back(createConstantIndex(builder, loc, value));
  return result;
}

static Value blockSizeForPhysicalDim(const DbPhysicalLayoutFacts &facts,
                                     unsigned physicalDim) {
  for (auto [idx, dim] : llvm::enumerate(facts.partitionedDims))
    if (dim == physicalDim && idx < facts.blockSizes.size())
      return facts.blockSizes[idx];
  return Value();
}

} // namespace

bool mlir::carts::arts::hasPhysicalDbLayoutFacts(Operation *op) {
  if (!op)
    return false;
  auto alloc = dyn_cast<DbAllocOp>(op);
  if (!alloc)
    return false;
  auto partition = alloc.getPartitionMode();
  return partition && usesBlockLayout(*partition) && !alloc.getSizes().empty();
}

FailureOr<DbPhysicalLayoutFacts>
mlir::carts::arts::resolvePhysicalDbLayoutFacts(ArrayAttr ownerDimsAttr,
                                                ArrayAttr blockShapeAttr,
                                                ValueRange elementSizes,
                                                OpBuilder &builder,
                                                Location loc) {
  if (!ownerDimsAttr || !blockShapeAttr || elementSizes.empty())
    return failure();

  std::optional<SmallVector<int64_t, 4>> ownerDims =
      readI64ArrayAttr(ownerDimsAttr);
  std::optional<SmallVector<int64_t, 4>> blockShape =
      readI64ArrayAttr(blockShapeAttr);
  if (!ownerDims || ownerDims->empty() || !blockShape || blockShape->empty())
    return failure();

  const unsigned memrefRank = elementSizes.size();
  DbPhysicalLayoutFacts facts(PartitionMode::block);

  llvm::DenseSet<unsigned> seenDims;
  for (int64_t rawDim : *ownerDims) {
    if (rawDim < 0 || static_cast<uint64_t>(rawDim) >= memrefRank)
      return failure();
    unsigned dim = static_cast<unsigned>(rawDim);
    if (!seenDims.insert(dim).second)
      return failure();
    facts.partitionedDims.push_back(dim);
  }

  SmallVector<Value, 4> blockShapeValues =
      materializeIndexConstants(builder, loc, *blockShape);
  for (auto [ownerSlot, dim] : llvm::enumerate(facts.partitionedDims)) {
    Value blockSize;
    if (blockShapeValues.size() == memrefRank) {
      blockSize = blockShapeValues[dim];
    } else {
      if (ownerSlot < blockShapeValues.size())
        blockSize = blockShapeValues[ownerSlot];
    }
    if (!blockSize)
      return failure();
    auto folded = arts::tryFoldConstantIndex(blockSize);
    if (folded && *folded <= 0)
      return failure();
    facts.blockSizes.push_back(blockSize);
  }

  if (facts.blockSizes.size() != facts.partitionedDims.size())
    return failure();

  for (auto [idx, physicalDim] : llvm::enumerate(facts.partitionedDims)) {
    Value extent = elementSizes[physicalDim];
    Value blockSize = facts.blockSizes[idx];
    if (!extent || !blockSize)
      return failure();
    facts.outerSizes.push_back(
        ceilDivPositiveIndex(builder, loc, extent, blockSize));
  }

  facts.innerSizes.reserve(memrefRank);
  for (unsigned dim = 0; dim < memrefRank; ++dim) {
    if (Value blockSize = blockSizeForPhysicalDim(facts, dim)) {
      facts.innerSizes.push_back(blockSize);
      continue;
    }
    facts.innerSizes.push_back(elementSizes[dim]);
  }

  if (!facts.isValid())
    return failure();
  return facts;
}
