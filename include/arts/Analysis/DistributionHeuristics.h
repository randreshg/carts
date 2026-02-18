///==========================================================================///
/// File: DistributionHeuristics.h
///
/// Machine-aware work distribution heuristics for the CARTS compiler.
///
/// This module decides HOW loop iterations are distributed across workers
/// in a distributed task-based runtime. It analyzes the machine topology
/// (nodes, threads per node, network threads) and selects a distribution
/// strategy that maximizes utilization at every level of the hierarchy.
///
/// Heuristic Family:
///   H2: Distribution - Decide iteration-to-worker mapping
///
/// H2 Distribution Heuristics:
///   H2.1: Machine Analysis    - Inspect topology -> select strategy
///   H2.2: Compute Bounds      - Emit IR for worker iteration ranges
///   H2.3: Recompute Inside    - Recompute bounds inside task EDT (SSA)
///
///===----------------------------------------------------------------------===///
/// Strategies
///===----------------------------------------------------------------------===///
///
/// Flat (single-level):
///   Used for intranode (single-node) execution.
///   All W workers divide N iterations equally using balanced distribution:
///     base = N / W,  rem = N % W
///     worker i gets: start = i*base + min(i, rem), count = base + (i < rem)
///
///   DB acquire and iteration bounds are identical per worker.
///
///   Example: N=100 iterations, W=4 workers
///     Worker 0: iter=[0, 25)    acquire=[0, 25)
///     Worker 1: iter=[25, 50)   acquire=[25, 50)
///     Worker 2: iter=[50, 75)   acquire=[50, 75)
///     Worker 3: iter=[75, 100)  acquire=[75, 100)
///
/// TwoLevel (inter-node + intra-node):
///   Used for internode (multi-node) execution.
///   Distributes work in two hierarchical levels:
///
///   Level 1 - Inter-node (DB block assignment):
///     Assigns iteration chunks to NODES. Each node gets one or more
///     contiguous DB blocks. All worker threads on the same node share the
///     same DB acquire range (reducing remote data transfers).
///
///     blockSize = ceil(arrayDim / numNodes)     [matches DbPartitioning]
///     totalChunks = ceil(totalIterations / blockSize)
///     (nodeStart, nodeChunks) = balancedDistribute(totalChunks, numNodes,
///                                                  nodeId)
///     nodeRows = nodeChunks * blockSize  (clamped to remaining iterations)
///
///   Level 2 - Intra-node (thread sub-division):
///     Within each node's block, further divides rows across local worker
///     threads. This ensures ALL threads on ALL nodes have work.
///
///     subBlock = ceil(nodeRows / workersPerNode)
///     localStart = localThreadId * subBlock
///     localCount = min(subBlock, nodeRows - localStart)
///
///   DB acquire uses Level 1 (node-level) bounds.
///   Inner loop uses Level 2 (thread-level) bounds.
///
///   Example: N=255 iterations, 6 nodes, 2 workers/node = 12 total workers
///            blockSize = ceil(256/6) = 43, totalChunks = 6
///
///     Node 0 (workers 0,1):  acquire=[0, 43)
///       Thread 0: iter=[0, 22)     Thread 1: iter=[22, 43)
///     Node 1 (workers 2,3):  acquire=[43, 86)
///       Thread 0: iter=[43, 65)    Thread 1: iter=[65, 86)
///     Node 2 (workers 4,5):  acquire=[86, 129)
///       Thread 0: iter=[86, 108)   Thread 1: iter=[108, 129)
///     Node 3 (workers 6,7):  acquire=[129, 172)
///       Thread 0: iter=[129, 151)  Thread 1: iter=[151, 172)
///     Node 4 (workers 8,9):  acquire=[172, 215)
///       Thread 0: iter=[172, 194)  Thread 1: iter=[194, 215)
///     Node 5 (workers 10,11): acquire=[215, 255)
///       Thread 0: iter=[215, 235)  Thread 1: iter=[235, 255)
///
///     All 6 nodes active, all 12 workers busy.
///
///   Why TwoLevel applies to ALL internode arrays (not just stencil):
///     - Stencil arrays: Fixes idle workers. Halo logic requires workers on
///       the same node to map to the same DB block.
///     - Block arrays: All workers on a node acquire the SAME block ->
///       reduces remote acquire count, improves cache sharing.
///     - Unified code path: simpler than separate stencil/block paths.
///
///===----------------------------------------------------------------------===///
/// Key invariants
///===----------------------------------------------------------------------===///
///
///   1. Worker ID mapping:
///        nodeId        = workerId / workersPerNode
///        localThreadId = workerId % workersPerNode
///      This must match EDT routing in ForLowering.
///
///   2. DB alignment:
///        blockSize = ceil(arrayDim / numNodes)
///      Must match DbPartitioning's block size so that ForLowering chunk
///      boundaries align with DB block boundaries.
///
///   3. Stencil safety:
///        All workers on the same node have baseOffset within the same
///        DB block, so blockIdx = baseOffset / planBlockSize is identical
///        for all threads on a node.
///
///   4. Balanced distribution:
///        Uses floor+remainder pattern to avoid idle tail workers:
///          base = total / n,  rem = total % n
///          participant i: start = i*base + min(i, rem)
///                         count = base + (i < rem ? 1 : 0)
///
///==========================================================================///

#ifndef ARTS_ANALYSIS_DISTRIBUTIONHEURISTICS_H
#define ARTS_ANALYSIS_DISTRIBUTIONHEURISTICS_H

#include "arts/ArtsDialect.h"
#include "arts/Utils/AbstractMachine/ArtsAbstractMachine.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

class ArtsCodegen;
class LoopAnalysis;

/// H2: Distribution strategy kinds
enum class DistributionKind {
  Flat,        ///< Single-level: all workers divide iterations equally
  TwoLevel,    ///< Two-level: nodes get DB blocks, threads subdivide within
  BlockCyclic, ///< Cyclic chunks: chunk k -> worker (k % totalWorkers)
  Tiling2D ///< Matmul-oriented 2D worker grid (row ownership + column striping)
};

/// Machine topology analysis result (compile-time, no IR)
struct DistributionStrategy {
  DistributionKind kind = DistributionKind::Flat;
  int64_t numNodes = 1;        ///< Total nodes
  int64_t workersPerNode = 1;  ///< Worker threads per node
  int64_t totalWorkers = 1;    ///< numNodes * workersPerNode
  bool useDbAlignment = false; ///< blockSize > 1 for DB boundary alignment
};

/// Runtime distribution bounds (SSA Values emitted during lowering)
struct DistributionBounds {
  /// Thread-level: inner loop bounds
  Value iterStart;     ///< First iteration for this worker
  Value iterCount;     ///< Actual iteration count
  Value iterCountHint; ///< Max possible (for sizing)
  Value hasWork;       ///< i1: does this worker have iterations?

  /// Acquire-level: DB acquire bounds
  /// For Flat: same as thread-level
  /// For TwoLevel: node-level (all threads on same node share same acquire)
  Value acquireStart;    ///< First iteration of acquire range
  Value acquireSize;     ///< Actual size
  Value acquireSizeHint; ///< Max possible (partition_sizes hint)
};

/// Resolved worker topology for an EDT.
struct WorkerConfig {
  int64_t totalWorkers = 0;
  int64_t workersPerNode = 0;
  bool internode = false;
};

/// 2D worker grid for tiling_2d strategy.
/// workerId = rowWorkerId * colWorkers + colWorkerId
struct Tiling2DWorkerGrid {
  Value rowWorkers;
  Value colWorkers;
  Value rowWorkerId;
  Value colWorkerId;
};

class DistributionHeuristics {
public:
  /// H2.1: Analyze machine topology -> distribution strategy
  /// Pure analysis, no IR emission.
  static DistributionStrategy
  analyzeStrategy(EdtConcurrency concurrency,
                  const ArtsAbstractMachine *machine = nullptr);

  /// Select IR distribution kind from machine strategy + detected loop pattern.
  static EdtDistributionKind
  selectDistributionKind(const DistributionStrategy &strategy,
                         EdtDistributionPattern pattern);

  /// Convert IR-level distribution enum into lowering strategy enum.
  static DistributionKind toDistributionKind(EdtDistributionKind kind);

  /// H2.2: Compute runtime bounds for a worker
  /// Emits arith MLIR ops via ArtsCodegen.
  /// totalWorkers: runtime SSA Value for total worker count.
  /// workersPerNode: runtime SSA Value for workers per node (TwoLevel only).
  ///   For Flat, this parameter is ignored and may be null.
  static DistributionBounds computeBounds(ArtsCodegen *AC, Location loc,
                                          const DistributionStrategy &strategy,
                                          Value workerId, Value totalWorkers,
                                          Value workersPerNode,
                                          Value totalIterations,
                                          Value totalChunks, Value blockSize);

  /// H2.3: Recompute bounds inside task EDT (handles SSA dominance)
  /// Clones loop bounds into current region, recomputes distribution.
  /// workersPerNode: runtime SSA Value for workers per node (TwoLevel only).
  static DistributionBounds recomputeBoundsInside(
      ArtsCodegen *AC, Location loc, const DistributionStrategy &strategy,
      Value workerId, Value insideTotalWorkers, Value workersPerNode,
      Value upperBound, Value lowerBound, Value loopStep, Value blockSize,
      std::optional<int64_t> alignmentBlockSize, bool useRuntimeBlockAlignment);

  /// Compute DB alignment block size from parallel EDT dependencies.
  /// For internode: ceil(arrayDim / numNodes) for ALL arrays.
  /// For intranode: align stencil arrays and write-capable arrays whose
  /// element count is not evenly divisible by worker count.
  static Value computeDbAlignmentBlockSize(EdtOp parallelEdt,
                                           Value numPartitions, ArtsCodegen *AC,
                                           Location loc);

  /// Get workers-per-node runtime Value for internode EDTs.
  static Value getWorkersPerNode(OpBuilder &builder, Location loc,
                                 EdtOp parallelEdt);

  /// Get workers-per-node runtime Value for internode EDTs.
  static Value getWorkersPerNode(ArtsCodegen *AC, Location loc,
                                 EdtOp parallelEdt);

  /// Get total worker count as an index Value for the given EDT.
  /// Honors explicit workers attribute, otherwise uses runtime queries.
  static Value getTotalWorkers(OpBuilder &builder, Location loc,
                               EdtOp parallelEdt);

  /// Get total worker count as an index Value for the given EDT.
  /// Honors explicit workers attribute, otherwise uses runtime queries.
  static Value getTotalWorkers(ArtsCodegen *AC, Location loc,
                               EdtOp parallelEdt);

  /// Get dispatch worker count used by ParallelEdtLowering worker loops.
  /// Honors explicit workers attribute. Defaults to:
  ///   - internode: total nodes
  ///   - intranode: total workers
  static Value getDispatchWorkerCount(OpBuilder &builder, Location loc,
                                      EdtOp parallelEdt);

  /// Resolve compile-time worker topology for an EDT from attrs + machine.
  /// Returns nullopt when worker count is not computable.
  static std::optional<WorkerConfig>
  resolveWorkerConfig(EdtOp parallelEdt,
                      const ArtsAbstractMachine *machine = nullptr);

  /// Compute an optional coarsened block-size hint for arts.for loops.
  /// Returns nullopt when coarsening should be skipped.
  static std::optional<int64_t>
  computeCoarsenedBlockHint(ForOp forOp, LoopAnalysis &loopAnalysis,
                            const WorkerConfig &workerCfg);

  /// Choose compile-time column worker count for tiling_2d.
  /// Defaults to a near-square divisor of totalWorkers. When workersPerNode is
  /// provided, prefer divisors that keep a row-worker group within one node
  /// (colWorkers divides workersPerNode).
  static int64_t
  chooseTiling2DColumnWorkers(int64_t totalWorkers,
                              std::optional<int64_t> workersPerNode);

  /// Compute row/column worker decomposition for tiling_2d.
  /// Falls back to rowWorkers=totalWorkers, colWorkers=1 when totalWorkers is
  /// not compile-time constant.
  static Tiling2DWorkerGrid getTiling2DWorkerGrid(ArtsCodegen *AC, Location loc,
                                                  Value workerId,
                                                  Value totalWorkers,
                                                  Value workersPerNode = Value());

private:
  /// Balanced floor+remainder distribution
  static std::pair<Value, Value>
  balancedDistribute(ArtsCodegen *AC, Location loc, Value totalChunks,
                     Value numParticipants, Value participantId);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DISTRIBUTIONHEURISTICS_H
