///==========================================================================///
/// File: DbElementWiseIndexer.h
///
/// Index localizer for element-wise (fine-grained) datablock allocation.
/// Each element of the partitioned dimension gets its own datablock.
///
/// Partitioning policy:
/// - Element-wise is the default fallback when chunk hints are missing,
///   access is stencil/mixed/irregular, or chunking is not beneficial.
/// - This preserves concurrency at the cost of more datablocks.
///
/// Example (A[100][50], elemOffset=25):
///   Access A[27][10]:
///     dbRefIdx = 27 - 25 = 2
///     memrefIdx = 10
///
/// Equations:
///   dbRefIdx   = globalRow - elemOffset
///   memrefIdx  = remaining dimensions unchanged
///   localLinear = globalLinear - (elemOffset * stride)
///
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEINDEXER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEINDEXER_H

#include "arts/Transforms/Datablock/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Index localizer for element-wise (fine-grained) datablock allocation.
/// Implements subtraction-based offset adjustment for fine-grained parallelism.
///
/// Index Localization Formulas:
///   Multi-dimensional: dbRefIdx = globalRow - elemOffset
///                      memrefIdx = remaining dimensions unchanged
///   Linearized:        localLinear = globalLinear - (elemOffset * stride)
class DbElementWiseIndexer : public DbIndexerBase {
  Value elemOffset_;
  Value elemSize_;
  SmallVector<Value> oldElementSizes_;

public:
  DbElementWiseIndexer(Value elemOffset, Value elemSize, unsigned outerRank,
                       unsigned innerRank, ValueRange oldElementSizes = {});

private:
  /// Split indices into outer/db_ref and inner/memref groups.
  LocalizedIndices splitIndices(ValueRange globalIndices, OpBuilder &builder,
                                Location loc);

public:
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

  /// Transform a DbRefOp and its load/store users
  void transformDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                           OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  /// Localize indices against a fine-grained acquire view.
  LocalizedIndices localizeForFineGrained(ValueRange globalIndices,
                                          ValueRange acquireIndices,
                                          ValueRange acquireOffsets,
                                          OpBuilder &builder, Location loc);

  /// Transform a single load/store using db_ref + load/store pattern.
  void transformAccess(Operation *op, Value dbPtr, Type elementType,
                       ArtsCodegen &AC,
                       llvm::SetVector<Operation *> &opsToRemove);

  /// Transform a list of operations
  void transformOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

  /// Transform all uses of an allocation in the parent region
  void transformUsesInParentRegion(Operation *alloc, DbAllocOp dbAlloc,
                                   ArtsCodegen &AC,
                                   llvm::SetVector<Operation *> &opsToRemove);

  /// Element-wise localizeCoordinates scales offset by stride
  /// for linearized accesses
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEINDEXER_H
