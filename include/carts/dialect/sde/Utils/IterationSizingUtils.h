///==========================================================================///
/// File: IterationSizingUtils.h
///
/// SDE-owned iteration sizing helpers for source-level scheduling intent.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_ITERATIONSIZINGUTILS_H
#define CARTS_DIALECT_SDE_UTILS_ITERATIONSIZINGUTILS_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace mlir::carts::sde {

int64_t ceilDivPositive(int64_t value, int64_t divisor);

SmallVector<int64_t, 4> factorWorkersAcrossDims(int64_t workers,
                                                ArrayRef<int64_t> extents);

SmallVector<int64_t, 4>
factorStencilWorkersAcrossDims(int64_t workers, ArrayRef<int64_t> extents,
                               ArrayRef<int64_t> haloRadii);

/// Refine an existing physical block shape so owner dimensions expose at least
/// the target source-level concurrency implied by `targetComputeUnits` when
/// the owner extents permit it. This only caps owner-dim block extents; it
/// never coarsens existing DB/MU grain and never touches non-owner dimensions.
bool enforceOwnerBlockConcurrencyFloor(
    ArrayRef<int64_t> shape, ArrayRef<int64_t> ownerPhysicalDims,
    int64_t targetComputeUnits, SmallVectorImpl<int64_t> &physicalBlockShape);

/// Build a logical CU/worker slice over an already committed physical DB/MU
/// block shape. Owner dimensions are grouped only by whole physical blocks;
/// non-owner dimensions keep their physical extent. This preserves storage
/// grain while letting SDE commit a coarser real loop/CU dispatch shape when
/// the owner-block count substantially exceeds the target compute units.
bool buildBlockAlignedLogicalWorkerSlice(
    ArrayRef<int64_t> shape, ArrayRef<int64_t> ownerPhysicalDims,
    ArrayRef<int64_t> physicalBlockShape, int64_t targetComputeUnits,
    SmallVectorImpl<int64_t> &logicalWorkerSlice);

/// Ratio of halo-expanded tile volume to owned tile volume for a stencil
/// distribution. Captures the per-tile network footprint inflation caused by
/// perimeter halo reads. Returns +infinity if any owned dimension collapses
/// to zero. Used by distribution-time tile-bytes accounting.
long double estimateStencilExpandedTileRatio(ArrayRef<int64_t> extents,
                                             ArrayRef<int64_t> haloRadii,
                                             ArrayRef<int64_t> grid);

/// Owned byte footprint of a single physical tile. Returns 0 if any shape
/// dimension is non-positive or `elemBytes <= 0`. Multiplication saturates at
/// int64_t max so callers can compare against planning floors safely.
int64_t tilePayloadBytes(ArrayRef<int64_t> physicalBlockShape,
                         int64_t elemBytes);

/// Coarsen `workers` (downward, halving) until the resulting plan's owned
/// tile payload reaches `minTileBytes`, but never below `minWorkers`.
/// `rebuild` is invoked with each candidate worker count and must populate
/// `candidateShape` from scratch (returning true on success). Returns the final
/// worker target; never returns < 1.
int64_t coarsenWorkersToTileByteFloor(
    int64_t workers, ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    int64_t minTileBytes, int64_t minWorkers,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild);

Value buildLogicalWorkerCapacityValue(OpBuilder &builder, Location loc);

Value buildTripCountValue(OpBuilder &builder, Location loc, SdeSuIterateOp op);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_ITERATIONSIZINGUTILS_H
