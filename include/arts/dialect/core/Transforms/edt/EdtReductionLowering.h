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
#include "arts/dialect/core/Analysis/metadata/MetadataManager.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/DenseSet.h"
#include <optional>

namespace mlir {
namespace arts {

struct ReductionLoweringInfo {
  SmallVector<Value> reductionVars;
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
allocatePartialAccumulators(ArtsCodegen *AC, MetadataManager &metadataManager,
                            ForOp forOp, EdtOp parallelEdt, Location loc,
                            bool splitMode = false,
                            Value workerCountOverride = Value());

/// Create result EDT to combine partial accumulators into final result DBs.
void createResultEdt(ArtsCodegen *AC, MetadataManager &metadataManager,
                     ReductionLoweringInfo &redInfo, Location loc);

/// Copy final reduction results back and release temporary reduction DBs.
void finalizeReductionAfterEpoch(ArtsCodegen *AC,
                                 ReductionLoweringInfo &redInfo, Location loc);

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREDUCTIONLOWERING_H
