///==========================================================================///
/// File: DbTransforms.h
/// Unified transformations for datablock operations.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DBTRANSFORMS_H
#define ARTS_TRANSFORMS_DBTRANSFORMS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// DbAllocPromotion - Allocation Promotion Transformation
///
/// Encapsulates the transformation of a coarse-grained allocation to a
/// fine-grained (chunked or element-wise) allocation. All inputs are provided
/// at construction, and apply() executes the transformation.
///
/// Mode is derived automatically from rank comparison:
///   oldInnerRank == newInnerRank -> Chunked (div/mod on first index)
///   oldInnerRank > newInnerRank  -> Element-wise (redistribute indices)
///
/// Usage:
///   DbAllocPromotion promotion(oldAlloc, newOuterSizes, newInnerSizes,
///                              acquires, elementOffsets, elementSizes);
///   auto newAlloc = promotion.apply(builder);
///===----------------------------------------------------------------------===///
class DbAllocPromotion {
public:
  /// Constructor: all inputs provided upfront.
  DbAllocPromotion(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                   ValueRange newInnerSizes, ArrayRef<DbAcquireOp> acquires,
                   ArrayRef<Value> elementOffsets,
                   ArrayRef<Value> elementSizes);

  /// Apply the transformation: create new allocation and rewrite all uses.
  FailureOr<DbAllocOp> apply(OpBuilder &builder);

  /// Check if this is a chunked transformation (vs element-wise).
  bool isChunked() const { return isChunked_; }

private:
  DbAllocOp oldAlloc_;
  SmallVector<Value> newOuterSizes_;
  SmallVector<Value> newInnerSizes_;

  /// Partition info per acquire
  SmallVector<DbAcquireOp> acquires_;
  SmallVector<Value> elementOffsets_;
  SmallVector<Value> elementSizes_;

  /// Derived at construction from rank comparison
  bool isChunked_;
  Value chunkSize_;

  /// Rewrite a single acquire with its partition info
  void rewriteAcquire(DbAcquireOp acquire, Value elemOffset, Value elemSize,
                      DbAllocOp newAlloc, OpBuilder &builder);

  /// Rewrite a DbRefOp with transformed indices
  void rewriteDbRef(DbRefOp ref, DbAllocOp newAlloc, OpBuilder &builder);
};

///===----------------------------------------------------------------------===///
/// db - Datablock Access Pattern Transformations
///===----------------------------------------------------------------------===///
namespace db {

/// Check if allocation is coarse-grained (all sizes == 1).
bool isCoarseGrained(DbAllocOp alloc);

/// Rewrite memref access to use db_ref pattern.
bool rewriteAccessWithDbPattern(Operation *op, Value dbPtr, Type elementType,
                                unsigned outerCount, OpBuilder &builder,
                                llvm::SetVector<Operation *> &opsToRemove);

/// Rebase operation indices to acquired view's local coordinates.
bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                         Type elementType, OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove);

/// Rebase all users of acquired blockArg to local coordinates.
bool rebaseAllUsersToAcquireView(DbAcquireOp acquire, OpBuilder &builder);

} // namespace db

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DBTRANSFORMS_H
