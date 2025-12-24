///==========================================================================///
/// File: DbChunkedRewriter.h
///
/// Index rewriter for chunked datablock allocation.
/// Multiple elements are grouped into chunks for coarse-grained parallelism.
///
/// Partitioning policy:
/// - Chunked is selected only when chunk hints exist and the cost model
///   prefers chunking; otherwise we fall back to element-wise for concurrency.
/// - Twin-diff remains enabled unless disjoint access is proven after rewrite.
///
/// Example (A[100][50], chunkSize=25, startChunk=1):
///   Chunk 1 covers rows 25..49.
///   Access A[27][10]:
///     chunkIdx = 27 / 25 = 1
///     dbRefIdx = chunkIdx - startChunk = 0
///     localRow = 27 % 25 = 2
///     memrefIdx = [2, 10]
///
/// Equations (div/mod localization):
///   dbRefIdx  = (globalRow / chunkSize) - startChunk
///   memrefIdx = globalRow % chunkSize
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H

#include "arts/Transforms/Datablock/DbRewriterBase.h"

namespace mlir {
namespace arts {

/// Index rewriter for chunked datablock allocation.
/// Implements div/mod localization for coarse-grained parallelism.
class DbChunkedRewriter : public DbRewriterBase {
  Value chunkSize_;  ///< Elements per chunk
  Value startChunk_; ///< First chunk this partition acquires
  Value elemOffset_; ///< Element offset (for stride computation)

public:
  DbChunkedRewriter(Value chunkSize, Value startChunk, Value elemOffset,
                    unsigned outerRank, unsigned innerRank);

  /// Transform global multi-dimensional indices to local
  /// For chunked mode: dbRefIdx = global / chunkSize - startChunk
  ///                   memrefIdx = global % chunkSize
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// Transform a linearized global index (for flattened 1D memrefs)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  /// Rewrite a DbRefOp and its load/store users
  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// Rebase a list of operations
  void rebaseOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                 ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

  /// Rewrite all uses of an allocation in the parent region
  void rewriteUsesInParentRegion(Operation *alloc, DbAllocOp dbAlloc,
                                 ArtsCodegen &AC,
                                 llvm::SetVector<Operation *> &opsToRemove);

  /// Mode-aware coordinate localization
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
