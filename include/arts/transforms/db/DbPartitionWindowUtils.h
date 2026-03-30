///==========================================================================///
/// File: DbPartitionWindowUtils.h
///
/// Shared helpers for turning owner-aligned element windows into
/// dependency-aligned element windows before DB-space block conversion.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DB_DBPARTITIONWINDOWUTILS_H
#define ARTS_TRANSFORMS_DB_DBPARTITIONWINDOWUTILS_H

#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"

namespace mlir {
namespace arts {

struct ExpandedElementWindow {
  Value offset;
  Value size;
};

/// Expand an owner-local element window to cover the full dependency span
/// described by [minOffset, maxOffset]. The start is clamped at zero and the
/// end is clamped to totalExtent when it is available.
inline ExpandedElementWindow
expandElementWindowWithHalo(OpBuilder &builder, Location loc, Value elemOffset,
                            Value elemSize, Value totalExtent,
                            int64_t minOffset, int64_t maxOffset) {
  Value zero = arts::createConstantIndex(builder, loc, 0);

  Value start = elemOffset;
  if (minOffset < 0) {
    Value haloLeft = arts::createConstantIndex(builder, loc, -minOffset);
    Value canShift = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, start, haloLeft);
    Value shifted = builder.create<arith::SubIOp>(loc, start, haloLeft);
    start = builder.create<arith::SelectOp>(loc, canShift, shifted, zero);
  } else if (minOffset > 0) {
    Value shift = arts::createConstantIndex(builder, loc, minOffset);
    start = builder.create<arith::AddIOp>(loc, start, shift);
  }

  Value end =
      builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
  if (maxOffset > 0) {
    Value haloRight = arts::createConstantIndex(builder, loc, maxOffset);
    Value endPlusHalo = builder.create<arith::AddIOp>(loc, end, haloRight);
    end = totalExtent ? builder.create<arith::MinUIOp>(loc, endPlusHalo,
                                                        totalExtent)
                      : endPlusHalo;
  } else if (maxOffset < 0) {
    Value shrink = arts::createConstantIndex(builder, loc, -maxOffset);
    Value canShrink = builder.create<arith::CmpIOp>(
        loc, arith::CmpIPredicate::uge, end, shrink);
    Value shrunken = builder.create<arith::SubIOp>(loc, end, shrink);
    end = builder.create<arith::SelectOp>(loc, canShrink, shrunken, zero);
  }

  Value endAfterStart = builder.create<arith::CmpIOp>(
      loc, arith::CmpIPredicate::uge, end, start);
  Value rawSize = builder.create<arith::SubIOp>(loc, end, start);
  Value size =
      builder.create<arith::SelectOp>(loc, endAfterStart, rawSize, zero);
  return ExpandedElementWindow{start, size};
}

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DB_DBPARTITIONWINDOWUTILS_H
