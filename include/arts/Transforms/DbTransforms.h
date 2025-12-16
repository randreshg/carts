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

  ///===--------------------------------------------------------------------===///
  /// AcquireViewMap - Coordinate mapping for acquired datablock views
  ///
  /// Captures information needed to transform global indices to local indices
  /// relative to an acquired view. The transformation behavior depends on:
  ///   - Promotion mode (chunked vs element-wise) from parent DbAllocPromotion
  ///   - Whether the view is accessed via EDT block argument (already local)
  ///===--------------------------------------------------------------------===///
  struct AcquireViewMap {
    /// Number of leading dimensions pinned by DbAcquireOp::indices
    /// For these dimensions, local index is always 0
    unsigned numIndexedDims = 0;

    /// Offsets for sliced dimensions (from DbAcquireOp::offsets)
    /// In chunked mode, these are chunk-space offsets
    SmallVector<Value> sliceOffsets;

    /// Element-level offsets (from DbAcquireOp::offset_hints)
    /// Used to localize memref indices inside EDT for chunked mode
    SmallVector<Value> elementOffsets;

    /// Whether this map is for an EDT block argument
    /// Critical for chunked mode: EDT block args represent already-localized views
    bool isEdtBlockArg = false;

    /// Create coordinate map from acquire operation
    static AcquireViewMap fromAcquire(DbAcquireOp acquire,
                                      bool isEdtBlockArg = false);
  };

  /// Localize global indices to acquired view's local coordinates
  /// Mode-aware: handles chunked vs element-wise differently
  ///
  /// For chunked mode + EDT block arg: indices are already local, no transform
  /// For chunked mode + outside EDT: subtract chunk offset
  /// For element-wise: subtract element offset
  SmallVector<Value> localizeIndices(ArrayRef<Value> globalIndices,
                                     const AcquireViewMap &map,
                                     OpBuilder &builder, Location loc) const;

  /// Localize memref element indices using element offset hints
  /// For chunked mode inside EDT, element indices need adjustment
  /// Example: global index 17 with element offset 17 becomes local index 0
  SmallVector<Value> localizeElementIndices(ArrayRef<Value> globalIndices,
                                            const AcquireViewMap &map,
                                            OpBuilder &builder,
                                            Location loc) const;

  /// Rebase all users of an acquired EDT block argument to local coordinates
  /// Mode-aware replacement for db::rebaseAllUsersToAcquireView
  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder);
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
