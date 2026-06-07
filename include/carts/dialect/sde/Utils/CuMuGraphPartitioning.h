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

/// SDE-level MU hyperedge pressure. This is intentionally abstract: fanout is
/// the number of other CU partitions expected to observe a memory unit, and
/// traffic is element-space byte pressure. It does not name ranks, routes,
/// storage objects, or communication operations.
struct CuMuHyperedgePressure {
  int64_t remoteFanout = 0;
  int64_t trafficBytes = 0;
};

/// Typed SDE CU vertex. The id is local to the graph slice and denotes a
/// source compute unit or an expanded block-range compute unit.
struct CuMuGraphVertex {
  unsigned id = 0;
  int64_t workWeight = 1;
};

/// Typed SDE MU net over CU vertices. `pinCuIds` is the hyperedge pin set;
/// `weightBytes` is abstract element-space traffic/pressure.
struct CuMuGraphNet {
  unsigned id = 0;
  int64_t weightBytes = 0;
  ArrayRef<unsigned> pinCuIds;
};

/// Borrowing view of a typed CU/MU hypergraph. It is target-neutral: vertices
/// and nets carry work/byte weights only, not routes, target storage objects,
/// target tasks, or communication operations.
struct CuMuTypedHypergraph {
  ArrayRef<CuMuGraphVertex> vertices;
  ArrayRef<CuMuGraphNet> nets;
};

/// SDE-level memory-unit vertex. It describes element-space layout only:
/// no target storage objects, routes, ranks, or communication-operation names.
struct CuMuMemoryUnit {
  ArrayRef<int64_t> shape;
  ArrayRef<int64_t> ownerPhysicalDims;
  int64_t elementBytes = 0;
  int64_t abstractCommVolumeBytes = 0;
  ArrayRef<CuMuHyperedgePressure> hyperedges;
  CuMuTypedHypergraph typedHypergraph;
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
  /// Optional exact-CU block weights. If the count does not match a candidate
  /// plan, the scorer derives weights from that candidate's block geometry.
  ArrayRef<int64_t> cuWorkWeights;
};

/// Partition objective for the CU/MU graph. `targetTileBytes` is an SDE
/// memory-granularity target, not a hard legality rule: if satisfying it would
/// underfill the machine, the optimizer keeps the concurrency floor instead.
struct CuMuPartitionObjective {
  int64_t targetTileBytes = 0;
  /// Allowed work imbalance when locally refining typed CU/MU hypergraph
  /// assignments. This stays abstract SDE planning state.
  double workImbalanceTolerance = 0.10;
  /// Relative pressure for lambda-minus-one remote-MU fanout in the typed
  /// hypergraph refinement objective.
  double remoteFanoutWeight = 1.0;
};

/// Selected partition. The physical block shape defines MU block granularity;
/// `computeUnits` is the number of CU vertices implied by that block shape.
struct CuMuPartitionPlan {
  int64_t computeUnits = 1;
  int64_t exposedParallelism = 1;
  int64_t tilePayloadBytes = 0;
  int64_t maxCuWorkWeight = 1;
  double workImbalance = 0.0;
  double remoteFanoutScore = 0.0;
  double score = 0.0;
  SmallVector<int64_t, 4> physicalBlockShape;
  SmallVector<unsigned, 8> vertexToPart;
};

/// Build a deterministic contiguous CU-to-part assignment. This is the common
/// initial partition for typed-graph scoring and a seed for future refinement.
SmallVector<unsigned, 8> buildContiguousCuPartAssignment(unsigned vertexCount,
                                                         unsigned partCount);

/// Compute the weighted hypergraph cut in bytes using the standard
/// `sum(weight * (lambda - 1))` metric, where lambda is the number of distinct
/// parts touched by a MU net's pins.
int64_t computeCuMuHypergraphCutBytes(const CuMuTypedHypergraph &graph,
                                      ArrayRef<unsigned> vertexToPart);

/// Infer the number of compute-unit vertices implied by partitioning `shape`
/// along `ownerPhysicalDims` with `physicalBlockShape`.
int64_t inferCuCountFromMuPartition(ArrayRef<int64_t> shape,
                                    ArrayRef<int64_t> ownerPhysicalDims,
                                    ArrayRef<int64_t> physicalBlockShape);

/// Choose the best CU/MU partition among source-level CU counts selected from
/// the halving spine plus bounded divisor and remote-fanout-adjacent samples.
/// `rebuild` maps a candidate CU count to its MU block shape.
std::optional<CuMuPartitionPlan> chooseCuMuGraphPartition(
    const CuMuMemoryUnit &memory, const CuMuComputeUnitTarget &compute,
    const CuMuPartitionObjective &objective,
    ArrayRef<int64_t> initialPhysicalBlockShape,
    llvm::function_ref<bool(int64_t, SmallVectorImpl<int64_t> &)> rebuild);

} // namespace mlir::carts::sde

#endif // CARTS_DIALECT_SDE_UTILS_CUMUGRAPHPARTITIONING_H
