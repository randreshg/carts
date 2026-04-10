///==========================================================================///
/// File: DbElementWiseIndexer.h
///
/// Index localizer for element-wise (fine-grained) datablock allocation.
/// Each element of the partitioned dimension gets its own datablock entry
/// when outerRank > 0. When outerRank == 0 (coarse layout), this indexer
/// behaves as a trivial single-DB localizer and forwards indices unchanged.
///
/// IMPORTANT: elemOffsets are element COORDINATES from partition_indices,
/// NOT range offsets. They identify which element this EDT owns.
///
/// For fine-grained mode:
///   - elemOffsets = element coordinates (e.g., [%i] from depend(inout: A[i]))
///   - globalIndices = inner indices from load/store (e.g., [%j])
///   - Use elemOffsets DIRECTLY as db_ref indices
///   - Pass globalIndices through as memref indices
///
/// Example (A[100][50], fine-grained on dim 0):
///   EDT owns element %i from loop
///   Access: A[%i][%j]
///   elemOffsets = [%i]   (element coordinate from partition_indices)
///   globalIndices = [%j] (inner index from memref.load)
///   Result: db_ref[%i], memref[%j]
///
/// Equations:
///   dbRefIdx  = elemOffsets (element coordinates - use directly)
///   memrefIdx = globalIndices (inner indices - pass through)
///
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEINDEXER_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEINDEXER_H

#include "arts/dialect/core/Transforms/db/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Index localizer for element-wise (fine-grained) datablock allocation.
///
/// Key insight: partitionInfo.indices from partition_indices ARE element
/// coordinates, not range offsets. Use them DIRECTLY as db_ref indices.
///
/// Index Localization:
///   dbRefIdx  = partitionInfo.indices (element coordinates)
///   memrefIdx = globalIndices (inner indices from load/store)
class DbElementWiseIndexer : public DbIndexerBase {
  SmallVector<Value> oldElementSizes;

public:
  /// Constructor with PartitionInfo - the canonical way to create indexers.
  /// Uses partitionInfo.indices as element coordinates for fine-grained mode.
  DbElementWiseIndexer(const PartitionInfo &info, unsigned outerRank,
                       unsigned innerRank, ValueRange oldElementSizes = {});

  //===--------------------------------------------------------------------===//
  /// Pure virtual overrides: mode-specific index localization
  //===--------------------------------------------------------------------===//

  /// Transform global multi-dimensional indices to local
  /// For element-wise mode: split indices by outer/inner rank and
  /// subtract elemOffset from the first outer index.
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// Transform a linearized global index
  /// localLinear = globalLinear - (elemOffset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  //===--------------------------------------------------------------------===//
  /// Virtual hook overrides
  //===--------------------------------------------------------------------===//

  /// ElementWise stride detection uses oldElementSizes instead of MemRefType.
  std::pair<bool, Value> detectLinearizedStride(ValueRange indices,
                                                Type elementType,
                                                OpBuilder &builder,
                                                Location loc) override;

  /// Handle empty indices by delegating to transformAccess for scalar access.
  bool handleEmptyIndices(Operation *op, Value dbPtr, Type elementType,
                          ArtsCodegen &AC,
                          llvm::SetVector<Operation *> &opsToRemove) override;

  /// ElementWise uses localizeForFineGrained instead of plain localize
  /// when rewriting DbRefOp users.
  LocalizedIndices localizeForDbRefUser(ValueRange elementIndices,
                                        Type newElementType, OpBuilder &builder,
                                        Location loc) override;

  //===--------------------------------------------------------------------===//
  /// ElementWise-specific methods
  //===--------------------------------------------------------------------===//

  /// Localize indices against a fine-grained acquire view.
  LocalizedIndices localizeForFineGrained(ValueRange globalIndices,
                                          ValueRange acquireIndices,
                                          ValueRange acquireOffsets,
                                          OpBuilder &builder, Location loc);

  /// Transform a single load/store using db_ref + load/store pattern.
  void transformAccess(Operation *op, Value dbPtr, Type elementType,
                       ArtsCodegen &AC,
                       llvm::SetVector<Operation *> &opsToRemove);

  /// Element-wise localizeCoordinates scales offset by stride
  /// for linearized accesses
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc);

private:
  /// Split indices into outer/db_ref and inner/memref groups.
  LocalizedIndices splitIndices(ValueRange globalIndices, OpBuilder &builder,
                                Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEINDEXER_H
