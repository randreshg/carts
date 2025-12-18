///==========================================================================///
/// File: ViewCoordinateMap.h
///
/// Global to Local Coordinate Transformation for datablock views.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_VIEWCOORDINATEMAP_H
#define ARTS_TRANSFORMS_DATABLOCK_VIEWCOORDINATEMAP_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// ViewCoordinateMap - Global to Local Coordinate Transformation
///
/// DbAcquireOp specifies a view into a datablock with:
///   - indices: Pin specific elements in leading dimensions (reduces
///   dimensionality)
///   - offsets: Starting position for the acquired slice
///   - sizes: Extent of the acquired slice
///
/// Example: 2D array with indices=[%row], offsets=[%col_start], sizes=[%n]
///   Global: arr[%row, %col]
///   Local:  view[0, %col - %col_start]
///         dimension 0 is pinned (indexed), dimension 1 is sliced
///
/// Transformation rules:
///   - Indexed dimensions [0, numIndexedDims): local = 0
///   - Sliced dimensions [numIndexedDims, end):
///     * If globalIdx == offset: local = 0
///     * If globalIdx = offset + delta (provable): local = delta
///     * Otherwise: local = globalIdx - offset (explicit subtraction)
///
/// We always compute local = global - offset for sliced dimensions.
///===----------------------------------------------------------------------===///
struct ViewCoordinateMap {
  /// Number of leading dimensions pinned by DbAcquireOp::indices.
  /// For these dimensions, the local coordinate is always 0.
  unsigned numIndexedDims = 0;

  /// Offsets for sliced dimensions (from DbAcquireOp::offsets).
  /// Applied to dimensions starting at index numIndexedDims.
  /// local[d] = global[d] - sliceOffsets[d - numIndexedDims]
  SmallVector<Value> sliceOffsets;

  /// Create coordinate map from a DbAcquireOp.
  static ViewCoordinateMap fromAcquire(DbAcquireOp acquire);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_VIEWCOORDINATEMAP_H
