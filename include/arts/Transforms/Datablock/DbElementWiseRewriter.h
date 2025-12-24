///==========================================================================///
/// File: DbElementWiseRewriter.h
///
/// Index rewriter for element-wise (fine-grained) datablock allocation.
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

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H

#include "arts/Transforms/Datablock/DbRewriterBase.h"

namespace mlir {
namespace arts {

/// Index rewriter for element-wise (fine-grained) datablock allocation.
/// Implements subtraction-based offset adjustment for fine-grained parallelism.
///
/// Index Localization Formulas:
///   Multi-dimensional: dbRefIdx = globalRow - elemOffset
///                      memrefIdx = remaining dimensions unchanged
///   Linearized:        localLinear = globalLinear - (elemOffset * stride)
class DbElementWiseRewriter : public DbRewriterBase {
  Value elemOffset_;                ///< First element this partition owns
  Value elemSize_;                  ///< Number of elements owned
  SmallVector<Value> oldElementSizes_; ///< Old allocation's element sizes for stride

public:
  DbElementWiseRewriter(Value elemOffset, Value elemSize, unsigned outerRank,
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

  /// Rewrite a DbRefOp and its load/store users
  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// Localize indices against a fine-grained acquire view.
  LocalizedIndices localizeForFineGrained(ValueRange globalIndices,
                                          ValueRange acquireIndices,
                                          ValueRange acquireOffsets,
                                          OpBuilder &builder, Location loc);

  /// Rewrite a single load/store using db_ref + load/store pattern.
  void rewriteAccessWithDbPattern(Operation *op, Value dbPtr, Type elementType,
                                  ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove);

  /// Rebase a list of operations
  void rebaseOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                 ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

  /// Rewrite all uses of an allocation in the parent region
  void rewriteUsesInParentRegion(Operation *alloc, DbAllocOp dbAlloc,
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

#endif // ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
