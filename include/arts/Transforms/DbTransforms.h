///==========================================================================///
/// File: DbTransforms.h
/// Unified transformations for datablock operations.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DBTRANSFORMS_H
#define ARTS_TRANSFORMS_DBTRANSFORMS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// ViewCoordinateMap - Global to Local Coordinate Transformation
///
/// DbAcquireOp specifies a view into a datablock with:
///   - indices: Pin specific elements in leading dimensions (reduces dimensionality)
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
///   - Sliced dimensions [numIndexedDims, end): local = global - offset
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

class DbTransforms {
public:
  explicit DbTransforms(MLIRContext *context);

  /// Rewrite memref access to use db_ref pattern.
  bool rewriteAccessWithDbPattern(Operation *op, Value dbPtr, Type elementType,
                                  unsigned outerCount, OpBuilder &builder,
                                  llvm::SetVector<Operation *> &opsToRemove);

  /// Promote dimensions from elementSizes to sizes.
  DbAllocOp promoteAllocation(DbAllocOp alloc, int promoteCount,
                              OpBuilder &builder, bool trimLeadingOnes = false);

  /// Rewrite all uses of an allocation after promotion.
  LogicalResult rewriteAllUses(DbAllocOp oldAlloc, DbAllocOp newAlloc);

  /// Rebase operation indices to acquired view's local coordinates.
  bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                           Type elementType, OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove);

  /// Rebase all users of acquired blockArg to local coordinates.
  bool rebaseAllUsersToAcquireView(DbAcquireOp acquire, OpBuilder &builder);

  /// Check if allocation is coarse-grained (all sizes == 1).
  static bool isCoarseGrained(DbAllocOp alloc);

  /// Transform global indices to local (view) coordinates using the map.
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         const ViewCoordinateMap &map,
                                         OpBuilder &builder, Location loc);

private:
  bool createDbRefPattern(Operation *op, Value dbPtr, Type elementType,
                          ArrayRef<Value> outerIndices,
                          ArrayRef<Value> innerIndices, OpBuilder &builder,
                          llvm::SetVector<Operation *> &opsToRemove);

  std::pair<SmallVector<Value>, SmallVector<Value>>
  splitIndices(ValueRange indices, unsigned splitPoint, OpBuilder &builder,
               Location loc);

  SmallVector<Value> applyOffsets(ArrayRef<Value> indices,
                                  ArrayRef<Value> offsets, OpBuilder &builder,
                                  Location loc);

  MLIRContext *context_;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DBTRANSFORMS_H
