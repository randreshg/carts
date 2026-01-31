///==========================================================================///
/// File: DbBlockIndexer.h
///
/// Index localizer for block datablock allocation.
/// Multiple elements are grouped into blocks for coarse-grained parallelism.
///
/// Partitioning policy:
/// - Block is selected only when block hints exist and the cost model
///   prefers blocking; otherwise we fall back to element-wise for concurrency.
/// - Twin-diff remains enabled unless disjoint access is proven after rewrite.
///
/// Example (A[100][50], blockSize=25, startBlock=1):
///   Block 1 covers rows 25..49.
///   Access A[27][10]:
///     blockIdx = 27 / 25 = 1
///     dbRefIdx = blockIdx - startBlock = 0
///     localRow = 27 % 25 = 2
///     memrefIdx = [2, 10]
///
/// Equations (div/mod localization):
///   dbRefIdx  = (globalRow / blockSize) - startBlock
///   memrefIdx = globalRow % blockSize
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBBLOCKINDEXER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBBLOCKINDEXER_H

#include "arts/Transforms/Datablock/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Index localizer for block datablock allocation.
/// Implements div/mod localization for coarse-grained parallelism.
/// Supports N-dimensional block partitioning where multiple leading dimensions
/// use div/mod localization.
///
/// Uses partitionInfo.sizes for block sizes. The startBlocks are computed
/// by the rewriter as: startBlock = partitionInfo.offsets / blockSize.
///
/// For each partitioned dimension d:
///   dbRefIdx[d]  = (globalIdx[d] / blockSize[d]) - startBlock[d]
///   memrefIdx[d] = globalIdx[d] % blockSize[d]
class DbBlockIndexer : public DbIndexerBase {
  /// Block size per partitioned dimension
  SmallVector<Value> blockSizes;
  /// Original dimension indices for each partitioned dimension
  SmallVector<unsigned> partitionedDims;
  /// Start block per partitioned dimension (computed)
  SmallVector<Value> startBlocks;
  /// True when allocation outer sizes are all constant 1
  bool allocSingleBlock = false;
  /// True when the acquired range is guaranteed to fit within a single block
  bool acquireSingleBlock = false;
  /// Optional zero constant defined in a dominating scope
  Value dominantZero;

public:
  /// Constructor with PartitionInfo - the canonical way to create indexers.
  /// Uses partitionInfo.sizes for blockSizes. The startBlocks must be passed
  /// separately as they require division which needs a builder.
  DbBlockIndexer(const PartitionInfo &info, ArrayRef<Value> startBlocks,
                 unsigned outerRank, unsigned innerRank,
                 bool allocSingleBlock = false, bool acquireSingleBlock = false,
                 Value dominantZero = Value());

  /// Number of partitioned dimensions
  unsigned numPartitionedDims() const {
    return !partitionedDims.empty() ? partitionedDims.size()
                                    : blockSizes.size();
  }

  /// Transform global multi-dimensional indices to local
  /// For block mode: dbRefIdx = global / blockSize - startBlock
  ///                 memrefIdx = global % blockSize
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// Transform a linearized global index (for flattened 1D memrefs)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  /// Transform a DbRefOp and its load/store users
  void transformDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                           OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  /// Transform a list of operations
  void transformOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

  /// Transform all uses of an allocation in the parent region
  void transformUsesInParentRegion(Operation *alloc, DbAllocOp dbAlloc,
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

#endif // ARTS_TRANSFORMS_DATABLOCK_DBBLOCKINDEXER_H
