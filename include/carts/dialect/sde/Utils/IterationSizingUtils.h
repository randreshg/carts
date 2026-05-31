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

/// Ratio of halo-expanded tile volume to owned tile volume for a stencil
/// distribution. Captures the per-tile network footprint inflation caused by
/// perimeter halo reads. Returns +infinity if any owned dimension collapses
/// to zero. Used by distribution-time tile-bytes accounting.
long double estimateStencilExpandedTileRatio(ArrayRef<int64_t> extents,
                                             ArrayRef<int64_t> haloRadii,
                                             ArrayRef<int64_t> grid);

/// Halo-expanded byte footprint of a single stencil tile (owned bytes scaled
/// by the perimeter halo expansion ratio). `physicalBlockShape` is the owned
/// per-dim tile extent; `extents` is the full output extent vector;
/// `haloRadii` is the per-physical-dim halo radius (0 for non-owner dims).
/// Returns 0 if any dim is non-positive or `elemBytes <= 0` (signals "skip").
int64_t haloExpandedTileBytes(ArrayRef<int64_t> extents,
                              ArrayRef<int64_t> haloRadii,
                              ArrayRef<int64_t> physicalBlockShape,
                              int64_t elemBytes);

/// Owned byte footprint of a single physical tile. Returns 0 if any shape
/// dimension is non-positive or `elemBytes <= 0`. Multiplication saturates at
/// int64_t max so callers can compare against planning floors safely.
int64_t tilePayloadBytes(ArrayRef<int64_t> physicalBlockShape,
                         int64_t elemBytes);

/// Coarsen `workers` (downward, halving) until the resulting plan's owned
/// tile payload reaches `minTileBytes`, but never below `minWorkers`. This is
/// the generic non-stencil counterpart to `coarsenStencilWorkersToFloor`: it
/// raises tile payload while preserving a floor on exposed CDAG parallelism.
/// `rebuild` is invoked with each candidate worker count and must populate
/// `candidateShape` from scratch (returning true on success). Returns the
/// final worker target; never returns < 1.
int64_t coarsenWorkersToTileByteFloor(
    int64_t workers, ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    int64_t minTileBytes, int64_t minWorkers,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild);

/// Coarsen `workers` (downward, halving) until the resulting plan's
/// halo-expanded tile footprint meets `minTileBytes`. `rebuild` is invoked
/// with each candidate worker count and must populate `candidateShape` from
/// scratch (returning true on success). Returns the final worker target;
/// never returns < 1. No-op when `minTileBytes <= 0`, `workers <= 1`, or
/// `elemBytes <= 0`.
int64_t coarsenStencilWorkersToFloor(
    int64_t workers, ArrayRef<int64_t> extents, ArrayRef<int64_t> haloRadii,
    ArrayRef<int64_t> physicalBlockShape, int64_t elemBytes,
    int64_t minTileBytes,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild);

Value buildLogicalWorkerCapacityValue(OpBuilder &builder, Location loc);

Value buildTripCountValue(OpBuilder &builder, Location loc, SdeSuIterateOp op);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_ITERATIONSIZINGUTILS_H
