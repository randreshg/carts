///==========================================================================///
/// File: EdtRewriter.h
///
/// Acquire rewriting for creating per-worker DbAcquireOps during ForLowering.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_EDT_EDTREWRITER_H
#define ARTS_TRANSFORMS_EDT_EDTREWRITER_H

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// Input bundle for per-dependency acquire rewriting.
struct AcquireRewriteInput {
  ArtsCodegen *AC = nullptr;
  Location loc;
  DbAcquireOp parentAcquire;
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

/// Rewrite a parent acquire into a block-partitioned per-worker acquire.
/// When applyStencilHalo is true, offsets/sizes are expanded with halo
/// clamping for stencil access patterns.
DbAcquireOp rewriteAcquire(AcquireRewriteInput &input, bool applyStencilHalo);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_EDT_EDTREWRITER_H
