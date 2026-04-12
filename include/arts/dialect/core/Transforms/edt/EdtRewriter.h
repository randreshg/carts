///==========================================================================///
/// File: EdtRewriter.h
///
/// Acquire rewriting for creating per-worker DbAcquireOps during ForLowering.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREWRITER_H
#define ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREWRITER_H

#include "arts/Dialect.h"
#include "arts/dialect/core/Conversion/ArtsToLLVM/CodegenSupport.h"
#include "arts/dialect/core/Transforms/edt/WorkDistributionUtils.h"
#include "arts/utils/LoweringContractUtils.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace arts {

/// Input bundle for per-dependency acquire rewriting.
struct AcquireRewriteInput {
  ArtsCodegen *AC = nullptr;
  Location loc;
  DbAcquireOp parentAcquire;
  ArtsMode effectiveMode = ArtsMode::inout;
  Value rootGuid;
  Value rootPtr;
  Value acquireOffset;
  Value acquireSize;
  Value acquireHintSize;
  /// Additional per-dimension partition slices for N-D block partitioning.
  SmallVector<Value, 4> extraOffsets;
  SmallVector<Value, 4> extraSizes;
  SmallVector<Value, 4> extraHintSizes;
  /// Per-dimension element extents aligned with the rewritten worker window.
  /// The first entry corresponds to acquireOffset/acquireSize, trailing entries
  /// correspond to extraOffsets/extraSizes.
  SmallVector<Value, 4> dimensionExtents;
  /// Per-dimension stencil halo contract aligned with dimensionExtents.
  SmallVector<int64_t, 4> haloMinOffsets;
  SmallVector<int64_t, 4> haloMaxOffsets;
  Value step;
  bool stepIsUnit = true;
  bool singleElement = false;
  bool forceCoarse = false;
  bool preserveParentDepRange = false;
  Value stencilExtent;
};

/// Shared planner input for one task-local acquire rewrite.
struct TaskAcquireRewritePlanInput {
  ArtsCodegen *AC = nullptr;
  Location loc;
  DbAcquireOp parentAcquire;
  ArtsMode effectiveMode = ArtsMode::inout;
  Value rootGuid;
  Value rootPtr;
  bool forceCoarseRewrite = false;
  DistributionKind distributionKind = DistributionKind::Flat;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<Tiling2DWorkerGrid> tiling2DGrid;
  LoweringContractInfo contract;
  Value acquireOffset;
  Value acquireSize;
  Value acquireHintSize;
  Value step;
  bool stepIsUnit = true;
};

/// Shared planner output for one task-local acquire rewrite.
struct TaskAcquireRewritePlan {
  AcquireRewriteInput rewriteInput;
  bool useStencilRewriter = false;
  std::optional<SmallVector<int64_t, 4>> refinedTaskBlockShape;
};

/// Shared input for patching task-local DB slice metadata after acquire
/// rewriting.
struct TaskAcquireSlicePlanInput {
  ArtsCodegen *AC = nullptr;
  Location loc;
  DbAcquireOp parentAcquire;
  DbAcquireOp taskAcquire;
  ArtsMode effectiveMode = ArtsMode::inout;
  Value rootGuid;
  Value rootPtr;
  DistributionKind distributionKind = DistributionKind::Flat;
  std::optional<EdtDistributionPattern> distributionPattern;
  Value plannedElementOffset;
  Value plannedElementSize;
  Value plannedElementSizeSeed;
  bool usesStencilHalo = false;
  LoweringContractInfo contract;
};

/// Plan the per-worker acquire rewrite payload from the resolved distribution
/// and semantic contract.
TaskAcquireRewritePlan
planTaskAcquireRewrite(TaskAcquireRewritePlanInput input);

/// Rewrite a parent acquire into a block-partitioned per-worker acquire.
/// When applyStencilHalo is true, offsets/sizes are expanded with halo
/// clamping for stencil access patterns.
DbAcquireOp rewriteAcquire(AcquireRewriteInput &input, bool applyStencilHalo);

/// Apply any task-local DB-space slice updates required after rewriteAcquire.
void applyTaskAcquireSlicePlan(TaskAcquireSlicePlanInput input);

/// Project semantic contract metadata onto one rewritten task acquire.
void applyTaskAcquireContractMetadata(
    Operation *semanticSourceOp, DbAcquireOp taskAcquire,
    const LoweringContractInfo &contract,
    std::optional<SmallVector<int64_t, 4>> refinedTaskBlockShape,
    OpBuilder &builder, Location loc);

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_EDT_EDTREWRITER_H
