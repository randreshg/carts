///==========================================================================///
/// File: ReductionLowering.h
///
/// Shared reduction-lowering helpers used by ForLowering.
///
/// Centralizes:
///   1) partial accumulator allocation
///   2) reduction result EDT materialization
///   3) post-epoch finalization and cleanup
///==========================================================================///

#ifndef ARTS_TRANSFORMS_EDT_REDUCTIONLOWERING_H
#define ARTS_TRANSFORMS_EDT_REDUCTIONLOWERING_H

#include "arts/ArtsDialect.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/DenseSet.h"
#include <optional>

namespace mlir {
namespace arts {

struct ReductionLoweringInfo {
  SmallVector<Value> reductionVars;
  Attribute loopMetadataAttr;
  std::optional<Location> loopLocation;

  /// Partial accumulators: array[num_workers] for intermediate results
  SmallVector<Value> partialAccumGuids, partialAccumPtrs, partialAccumArgs;

  /// Final result accumulators: scalar for each reduction variable
  SmallVector<Value> finalResultGuids, finalResultPtrs, finalResultArgs;

  SmallVector<bool> finalResultIsExternal;

  /// Optional stack-allocated sinks to mirror final reduction value
  SmallVector<Value> hostResultPtrs;

  Value numWorkers;
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
                            Location loc, Attribute loopMetadataAttr,
                            bool splitMode = false);

/// Create result EDT to combine partial accumulators into final result DBs.
void createResultEdt(ArtsCodegen *AC, ReductionLoweringInfo &redInfo,
                     Location loc);

/// Copy final reduction results back and release temporary reduction DBs.
void finalizeReductionAfterEpoch(ArtsCodegen *AC,
                                 ReductionLoweringInfo &redInfo, Location loc);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_EDT_REDUCTIONLOWERING_H
