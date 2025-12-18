///==========================================================================///
/// File: DbTransforms.h
/// Unified transformations for datablock operations.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DBTRANSFORMS_H
#define ARTS_TRANSFORMS_DBTRANSFORMS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"
#include <memory>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// PartitionDescriptor - Complete description of how a datablock is partitioned
///
/// Pure data structure providing a SINGLE SOURCE OF TRUTH for all index
/// localization operations. Created and consumed within DbAllocPromotion
/// transformation scope.
///
/// Key Concepts:
/// - GlobalIndex: Index in the original (unpromoted) allocation
/// - LocalIndex: Index within this partition's acquired view (starts at 0)
/// - ChunkIndex: Which chunk within the partition (for chunked mode)
/// - IntraChunkIndex: Position within a single chunk (for chunked mode)
///
/// This class fixes the bug where linearized memref offsets were not scaled
/// by stride, causing incorrect memory access patterns.
///===----------------------------------------------------------------------===///
class PartitionDescriptor {
public:
  /// Partition strategy - HOW was the allocation partitioned?
  enum class Strategy {
    None,       // Not partitioned - single chunk containing everything
    Chunked,    // Fixed-size chunks (div/mod transformation)
    ElementWise // Each element/row is a separate chunk
  };

  Strategy strategy = Strategy::None;

  /// The dimension along which partitioning occurs (0 = outermost)
  /// ONLY this dimension needs index transformation; others pass through
  unsigned partitionDim = 0;

  /// First element this partition owns (global element index)
  Value globalStartElement;

  /// Number of elements this partition owns
  Value elementCount;

  /// Physical chunk size (elements per chunk) - for chunked mode
  Value chunkSize;

  /// First chunk this partition acquires (in global chunk space) - for chunked
  Value globalStartChunk;

  //===--------------------------------------------------------------------===//
  // Factory Methods
  //===--------------------------------------------------------------------===//

  /// Create descriptor for chunked partition
  static PartitionDescriptor forChunked(Value chunkSize, Value startChunk,
                                        Value startElement, Value elementCount,
                                        unsigned partitionDim = 0);

  /// Create descriptor for element-wise partition
  static PartitionDescriptor forElementWise(Value startElement,
                                            Value elementCount,
                                            unsigned partitionDim = 0);

  //===--------------------------------------------------------------------===//
  // Index Transformation API - The Core Interface
  //===--------------------------------------------------------------------===//

  /// Result of localizing indices
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  // Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; // Indices for memref load/store
  };

  /// Transform global multi-dimensional indices to local
  ///
  /// INPUT: globalIndices in original allocation's coordinate space
  /// OUTPUT: {dbRefIndices, memrefIndices} for the promoted structure
  ///
  /// For partitionDim:
  ///   Chunked: dbRef = (global / chunkSize) - globalStartChunk
  ///            memref = global % chunkSize
  ///   ElementWise: dbRef = global - globalStartElement
  ///                memref = (remaining dims)
  /// Other dims: pass through unchanged
  LocalizedIndices localize(ArrayRef<Value> globalIndices, unsigned newOuterRank,
                            unsigned newInnerRank, OpBuilder &builder,
                            Location loc) const;

  /// Transform a linearized global index (for flattened 1D memrefs)
  ///
  /// For flattened memrefs where index = row*stride + col
  /// The KEY fix: scales offset by stride before subtracting!
  ///
  /// CRITICAL FIX for bug:
  ///   WRONG:   localLinear = globalLinear - offset
  ///   CORRECT: localLinear = globalLinear - (offset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      unsigned newOuterRank,
                                      unsigned newInnerRank, OpBuilder &builder,
                                      Location loc) const;

  //===--------------------------------------------------------------------===//
  // Query Helpers
  //===--------------------------------------------------------------------===//

  bool isChunked() const { return strategy == Strategy::Chunked; }
  bool isElementWise() const { return strategy == Strategy::ElementWise; }
  bool isPartitioned() const { return strategy != Strategy::None; }
};

///===----------------------------------------------------------------------===///
/// DbRewriter - Abstract interface for index localization
///
/// Strategy pattern: separate implementations for Chunked vs ElementWise.
/// ALL code paths use this interface, ensuring consistent behavior.
///
/// Key benefits over scattered if/else:
/// - Mode encapsulated: caller doesn't know/care if chunked or element-wise
/// - Forces all code through one path: no way to skip stride scaling
/// - Debug-friendly: LLVM_DEBUG in one place covers all uses
/// - N-dimensional support: each implementation handles any rank
///===----------------------------------------------------------------------===///
class DbRewriter {
public:
  virtual ~DbRewriter() = default;

  //===--------------------------------------------------------------------===//
  // Core Index Transformation API
  //===--------------------------------------------------------------------===//

  /// Result of localizing indices
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  // Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; // Indices for memref load/store
  };

  /// Transform global multi-dimensional indices to local
  /// INPUT: globalIndices in original allocation's coordinate space
  /// OUTPUT: {dbRefIndices, memrefIndices} for the promoted structure
  virtual LocalizedIndices localize(ArrayRef<Value> globalIndices,
                                    OpBuilder &builder, Location loc) = 0;

  /// Transform a linearized global index (for flattened 1D memrefs)
  /// Handles stride scaling correctly for each mode
  virtual LocalizedIndices localizeLinearized(Value globalLinearIndex,
                                              Value stride, OpBuilder &builder,
                                              Location loc) = 0;

  //===--------------------------------------------------------------------===//
  // Higher-level Rewrite Operations
  //===--------------------------------------------------------------------===//

  /// Rewrite a DbRefOp and its load/store users
  virtual void rewriteDbRefUsers(DbRefOp ref, Value blockArg,
                                 Type newElementType, OpBuilder &builder,
                                 llvm::SetVector<Operation *> &opsToRemove) = 0;

  //===--------------------------------------------------------------------===//
  // Mode-aware Coordinate Localization (replaces standalone functions)
  //===--------------------------------------------------------------------===//

  /// Mode-aware coordinate localization (replaces standalone localizeCoordinates)
  /// Handles linearized detection and stride scaling correctly for each mode
  virtual SmallVector<Value>
  localizeCoordinates(ArrayRef<Value> globalIndices,
                      ArrayRef<Value> sliceOffsets, unsigned numIndexedDims,
                      Type elementType, OpBuilder &builder, Location loc) = 0;

  /// Mode-aware rebase for a single operation (replaces db::rebaseToAcquireView)
  virtual bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire,
                                   Value dbPtr, Type elementType,
                                   OpBuilder &builder,
                                   llvm::SetVector<Operation *> &opsToRemove) = 0;

  /// Mode-aware rebase for all EDT users (replaces db::rebaseAllUsersToAcquireView)
  virtual bool rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                           OpBuilder &builder) = 0;

  //===--------------------------------------------------------------------===//
  // Factory Method
  //===--------------------------------------------------------------------===//

  /// Create appropriate rewriter based on mode
  /// oldElementSizes: For element-wise mode, the old allocation's element sizes
  ///                  for proper stride computation (supports dynamic strides)
  static std::unique_ptr<DbRewriter>
  create(bool isChunked, Value chunkSize, Value startChunk, Value elemOffset,
         Value elemSize, unsigned newOuterRank, unsigned newInnerRank,
         ValueRange oldElementSizes = {});
};

///===----------------------------------------------------------------------===///
/// ChunkedRewriter - Implementation for chunked (div/mod) mode
///
/// For chunked allocations:
///   dbRefIdx = (globalRow / chunkSize) - startChunk
///   memrefIdx = globalRow % chunkSize
///===----------------------------------------------------------------------===///
class ChunkedRewriter : public DbRewriter {
  Value chunkSize_;   // Elements per chunk
  Value startChunk_;  // First chunk this partition acquires
  Value elemOffset_;  // Element offset (for stride computation)
  unsigned outerRank_;
  unsigned innerRank_;

public:
  ChunkedRewriter(Value chunkSize, Value startChunk, Value elemOffset,
                  unsigned outerRank, unsigned innerRank);

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc) override;

  bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                           Type elementType, OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  bool rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                   OpBuilder &builder) override;
};

///===----------------------------------------------------------------------===///
/// ElementWiseRewriter - Implementation for element-wise (row-per-task) mode
///
/// For element-wise allocations:
///   dbRefIdx = globalRow - elemOffset
///   memrefIdx = remaining dimensions
///
/// THE KEY FIX: For linearized indices, scales offset by stride!
///   localLinear = globalLinear - (elemOffset * stride)
///===----------------------------------------------------------------------===///
class ElementWiseRewriter : public DbRewriter {
  Value elemOffset_; // First element this partition owns
  Value elemSize_;   // Number of elements owned
  unsigned outerRank_;
  unsigned innerRank_;
  SmallVector<Value> oldElementSizes_; // Old allocation's element sizes for stride

public:
  ElementWiseRewriter(Value elemOffset, Value elemSize, unsigned outerRank,
                      unsigned innerRank, ValueRange oldElementSizes = {});

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// THE KEY FIX: scales offset by stride before subtracting!
  /// localLinear = globalLinear - (elemOffset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// THE KEY FIX: Element-wise localizeCoordinates scales offset by stride
  /// for linearized accesses
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc) override;

  bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                           Type elementType, OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  bool rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                   OpBuilder &builder) override;
};

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

    /// For DISJOINT multi-chunk localization:
    /// Physical chunk size for div/mod operations (e.g., 66 rows)
    Value chunkSize;

    /// First acquired chunk index for relative db_ref indexing
    /// db_ref index = (globalRow / chunkSize) - startChunk
    Value startChunk;

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
  /// For chunked mode, startChunk should be passed from rewriteAcquire to avoid
  /// recomputation. If nullptr in chunked mode, falls back to reading from hints.
  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value startChunk = nullptr);
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
