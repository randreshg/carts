///==========================================================================///
/// File: EdtReductionLowering.h
///
/// Shared reduction-lowering helpers used by ForLowering.
///
/// Centralizes:
///   1) partial accumulator allocation
///   2) reduction result EDT materialization
///   3) post-epoch finalization and cleanup
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREDUCTIONLOWERING_H
#define ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREDUCTIONLOWERING_H

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/DenseSet.h"
#include <optional>

namespace mlir {
namespace arts {

enum class ReductionLoweringStrategy { localAccumulate, tree, atomic };

/// Reduction combiner kind for lowering. Must match SdeReductionKind enum
/// values so that integer attr values read from arts.reduction_kinds can be
/// cast directly.
enum class ReductionCombinerKind : int32_t {
  add = 0,
  mul = 1,
  min = 2,
  max = 3,
  land = 4,
  lor = 5,
  lxor = 6
};

struct ReductionLoweringInfo {
  SmallVector<Value> reductionVars;
  SmallVector<Value> privateReductionAccums;
  SmallVector<ReductionCombinerKind> combinerKinds;
  std::optional<Location> loopLocation;

  /// Partial accumulators: array[num_workers] for intermediate results
  SmallVector<Value> partialAccumGuids, partialAccumPtrs, partialAccumArgs;

  /// Final result accumulators: scalar for each reduction variable
  SmallVector<Value> finalResultGuids, finalResultPtrs, finalResultArgs;

  SmallVector<bool> finalResultIsExternal;

  /// Optional stack-allocated sinks to mirror final reduction value
  SmallVector<Value> hostResultPtrs;

  Value numWorkers;
  bool combineDirectlyInTask = false;
  EdtConcurrency parentConcurrency = EdtConcurrency::intranode;
  ReductionLoweringStrategy strategy =
      ReductionLoweringStrategy::localAccumulate;
};

/// Detect block arguments used for reduction stores in the loop body.
DenseSet<Value> detectReductionBlockArgs(ForOp forOp);

/// Returns true if this parallel block argument is reduction-related.
bool shouldSkipReductionArg(BlockArgument parallelArg,
                            const ReductionLoweringInfo &redInfo,
                            const DenseSet<Value> &reductionBlockArgs);

/// Collect db_ref operations that use old accumulator block args.
/// These will be skipped during cloning, with results mapped to myAccumulator.
void collectOldAccumulatorDbRefs(ForOp forOp, Block &parallelBlock,
                                 const DenseSet<Value> &reductionBlockArgs,
                                 DenseSet<Operation *> &opsToSkip,
                                 IRMapping &mapper, Value myAccumulator);

/// Allocate reduction DBs and initialize worker-local partial accumulators.
ReductionLoweringInfo
allocatePartialAccumulators(ArtsCodegen *AC, ForOp forOp, EdtOp parallelEdt,
                            Location loc, bool splitMode = false,
                            Value workerCountOverride = Value());

/// Create result EDT to combine partial accumulators into final result DBs.
void createResultEdt(ArtsCodegen *AC, ReductionLoweringInfo &redInfo,
                     Location loc);

/// Copy final reduction results back and release temporary reduction DBs.
void finalizeReductionAfterEpoch(ArtsCodegen *AC,
                                 ReductionLoweringInfo &redInfo, Location loc);

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREDUCTIONLOWERING_H
