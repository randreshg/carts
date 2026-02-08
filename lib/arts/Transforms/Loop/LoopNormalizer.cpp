///==========================================================================///
/// LoopNormalizer.cpp - Shared utilities for loop normalization patterns
///==========================================================================///

#include "arts/Transforms/Loop/LoopNormalizer.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

void arts::copyLoopAttributes(Operation *source, Operation *target) {
  if (!source || !target)
    return;
  if (auto idAttr = source->getAttr(AttrNames::Operation::ArtsId))
    target->setAttr(AttrNames::Operation::ArtsId, idAttr);
  if (auto loopAttr = source->getAttr(AttrNames::LoopMetadata::Name))
    target->setAttr(AttrNames::LoopMetadata::Name, loopAttr);
}

void arts::updateTripCountMetadata(Operation *loop, int64_t newTripCount) {
  if (!loop)
    return;
  auto attr = loop->getAttrOfType<LoopMetadataAttr>(
      AttrNames::LoopMetadata::Name);
  if (!attr)
    return;

  auto *ctx = loop->getContext();
  auto updated = LoopMetadataAttr::get(
      ctx, attr.getPotentiallyParallel(), attr.getHasReductions(),
      attr.getReductionKinds(),
      /*tripCount=*/IntegerAttr::get(IntegerType::get(ctx, 64), newTripCount),
      attr.getNestingLevel(), attr.getHasInterIterationDeps(),
      attr.getMemrefsWithLoopCarriedDeps(), attr.getParallelClassification(),
      attr.getLocationKey());
  loop->setAttr(AttrNames::LoopMetadata::Name, updated);
}

/// Strip arith.index_cast ops to find the underlying value.
static Value stripIndexCasts(Value v) {
  while (auto cast = v.getDefiningOp<arith::IndexCastOp>())
    v = cast.getIn();
  return v;
}

/// Check if two values refer to the same SSA value, looking through
/// index_cast operations.
static bool isSameValueModuloCasts(Value a, Value b) {
  return stripIndexCasts(a) == stripIndexCasts(b);
}

/// Try to extract a positive integer constant from a Value, looking through
/// index_cast and matching both arith.constant (any int type) and
/// arith.constant with IndexType.
static std::optional<int64_t> getPositiveConstant(Value v) {
  v = stripIndexCasts(v);
  if (auto constIdx = v.getDefiningOp<arith::ConstantIndexOp>())
    return constIdx.value() > 0 ? std::optional<int64_t>(constIdx.value())
                                : std::nullopt;
  if (auto constInt = v.getDefiningOp<arith::ConstantIntOp>()) {
    int64_t val = constInt.value();
    return val > 0 ? std::optional<int64_t>(val) : std::nullopt;
  }
  return std::nullopt;
}

std::optional<int64_t> arts::getTriangularOffset(Value lb, Value outerIV) {
  if (!lb || !outerIV)
    return std::nullopt;

  /// Look through index_cast on the lower bound to find the addi
  Value stripped = stripIndexCasts(lb);
  auto addOp = stripped.getDefiningOp<arith::AddIOp>();
  if (!addOp)
    return std::nullopt;

  /// Check addi(outerIV, const) or addi(const, outerIV),
  /// comparing through index_cast on both sides
  Value lhs = addOp.getLhs(), rhs = addOp.getRhs();
  Value constOperand;
  if (isSameValueModuloCasts(lhs, outerIV))
    constOperand = rhs;
  else if (isSameValueModuloCasts(rhs, outerIV))
    constOperand = lhs;
  else
    return std::nullopt;

  return getPositiveConstant(constOperand);
}
