///==========================================================================///
/// File: CuMuGraphPartitioning.h
///
/// SDE-owned CU/MU graph partitioning helpers.
///==========================================================================///

#ifndef CARTS_DIALECT_SDE_UTILS_CUMUGRAPHPARTITIONING_H
#define CARTS_DIALECT_SDE_UTILS_CUMUGRAPHPARTITIONING_H

#include "mlir/Support/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <optional>

namespace mlir::carts::sde {

/// SDE-level memory-unit vertex. It describes element-space layout only:
/// no runtime storage objects, routes, ranks, or collective names.
struct CuMuMemoryUnit {
  ArrayRef<int64_t> shape;
  ArrayRef<int64_t> ownerPhysicalDims;
  int64_t elementBytes = 0;
  int64_t abstractCommVolumeBytes = 0;
};

/// SDE-level compute-unit target. `requestedComputeUnits` is the initial
/// source-level CU wave count; `minComputeUnits` is the concurrency floor that
/// prevents graph reconstruction from serializing below machine parallelism.
struct CuMuComputeUnitTarget {
  int64_t requestedComputeUnits = 1;
  int64_t minComputeUnits = 1;
  int64_t logicalWorkerCapacity = 1;
  double taskCreationCost = 0.0;
  double taskSyncCost = 0.0;
  double dataAccessCost = 1.0;
};

/// Partition objective for the CU/MU graph. `targetTileBytes` is an SDE
/// memory-granularity target, not a hard legality rule: if satisfying it would
/// underfill the machine, the optimizer keeps the concurrency floor instead.
struct CuMuPartitionObjective {
  int64_t targetTileBytes = 0;
};

/// Selected partition. The physical block shape defines MU block granularity;
/// `computeUnits` is the number of CU vertices implied by that block shape.
struct CuMuPartitionPlan {
  int64_t computeUnits = 1;
  int64_t exposedParallelism = 1;
  int64_t tilePayloadBytes = 0;
  double score = 0.0;
  SmallVector<int64_t, 4> physicalBlockShape;
};

/// Infer the number of compute-unit vertices implied by partitioning `shape`
/// along `ownerPhysicalDims` with `physicalBlockShape`.
int64_t inferCuCountFromMuPartition(ArrayRef<int64_t> shape,
                                    ArrayRef<int64_t> ownerPhysicalDims,
                                    ArrayRef<int64_t> physicalBlockShape);

/// Choose the best CU/MU partition among the worker counts obtained by
/// repeatedly halving `requestedComputeUnits` down to `minComputeUnits`.
/// `rebuild` maps a candidate CU count to its MU block shape.
std::optional<CuMuPartitionPlan> chooseCuMuGraphPartition(
    const CuMuMemoryUnit &memory, const CuMuComputeUnitTarget &compute,
    const CuMuPartitionObjective &objective,
    ArrayRef<int64_t> initialPhysicalBlockShape,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_CUMUGRAPHPARTITIONING_H
