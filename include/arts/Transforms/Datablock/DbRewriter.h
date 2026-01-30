///==========================================================================///
/// File: DbRewriter.h
///
/// Abstract base class for datablock allocation transformation.
/// Provides factory pattern for creating mode-specific rewriters.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H

#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbIndexerBase.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===//
/// Partition mode utilities
///===----------------------------------------------------------------------===//

/// Check if mode requires block size parameter
inline bool requiresBlockSize(PartitionMode mode) {
  return mode == PartitionMode::block || mode == PartitionMode::stencil;
}

/// Get string name for mode (for debugging/logging)
inline StringRef getPartitionModeName(PartitionMode mode) {
  switch (mode) {
  case PartitionMode::coarse:
    return "Coarse";
  case PartitionMode::fine_grained:
    return "FineGrained";
  case PartitionMode::block:
    return "Block";
  case PartitionMode::stencil:
    return "Stencil";
  }
  return "Unknown";
}

///===----------------------------------------------------------------------===//
/// Stencil info for ESD halo-based localization
///===----------------------------------------------------------------------===//
struct StencilInfo {
  int64_t haloLeft = 0;
  int64_t haloRight = 0;
  Value totalRows;

  bool hasHalo() const { return haloLeft > 0 || haloRight > 0; }
};

/// Per-acquire rewrite input preserving partition semantics.
/// Supports multi-dimensional partitioning for fine-grained (A[i][j]) and
/// block modes.
struct DbRewriteAcquire {
  DbAcquireOp acquire;
  /// Canonical partition info - preserves semantic context (mode, indices,
  /// offsets, sizes) through the pipeline. Indexers use this to know whether
  /// values are element coordinates (indices) or range starts (offsets).
  ///
  /// Field semantics by mode:
  /// - fine_grained: indices = element COORDINATES (use directly as db_ref idx)
  /// - block/stencil: offsets = range START, sizes = range SIZE (for div/mod)
  PartitionInfo partitionInfo;
  bool isFullRange = false;
  bool skipRebase = false;

  /// Accessors for partition data (mode-aware)
  ArrayRef<Value> getIndices() const { return partitionInfo.indices; }
  ArrayRef<Value> getOffsets() const { return partitionInfo.offsets; }
  ArrayRef<Value> getSizes() const { return partitionInfo.sizes; }

  /// Legacy accessors for 1D backward compatibility
  Value getElemOffset() const {
    if (!partitionInfo.indices.empty())
      return partitionInfo.indices.front();
    return partitionInfo.offsets.empty() ? Value()
                                         : partitionInfo.offsets.front();
  }
  Value getElemSize() const {
    return partitionInfo.sizes.empty() ? Value() : partitionInfo.sizes.front();
  }
};

/// Rewriter plan chosen by DbPartitioning.
struct DbRewritePlan {
  PartitionMode mode = PartitionMode::fine_grained;
  std::optional<StencilInfo> stencilInfo;

  /// N-D block sizes (one per partitioned dimension)
  SmallVector<Value> blockSizes;

  /// Allocation shape (single source of truth)
  SmallVector<Value> outerSizes; /// Number of blocks per dimension
  SmallVector<Value> innerSizes; /// Size of each dimension within a block

  /// Mixed mode support
  bool isMixedMode = false;

  /// Derived ranks (computed from sizes)
  unsigned outerRank() const { return outerSizes.size(); }
  unsigned innerRank() const { return innerSizes.size(); }

  /// Get block size for dimension d
  Value getBlockSize(unsigned d = 0) const {
    return d < blockSizes.size() ? blockSizes[d] : Value();
  }

  /// Number of partitioned dimensions
  unsigned numPartitionedDims() const { return blockSizes.size(); }

  bool isValid() const {
    switch (mode) {
    case PartitionMode::stencil:
      return stencilInfo.has_value();
    case PartitionMode::block:
      return !blockSizes.empty();
    case PartitionMode::fine_grained:
    case PartitionMode::coarse:
      return true;
    }
    return false;
  }

  /// Default constructor
  DbRewritePlan() = default;
  explicit DbRewritePlan(PartitionMode m) : mode(m) {}
};

///===----------------------------------------------------------------------===//
/// DbRewriter - Abstract base for allocation transformation
///===----------------------------------------------------------------------===//
class DbRewriter {
public:
  virtual ~DbRewriter() = default;

  static std::unique_ptr<DbRewriter> create(DbAllocOp oldAlloc,
                                            ArrayRef<DbRewriteAcquire> acquires,
                                            DbRewritePlan &plan);
  FailureOr<DbAllocOp> apply(OpBuilder &builder);

  /// Accessors
  PartitionMode getMode() const { return plan.mode; }
  bool isElementWise() const {
    return plan.mode == PartitionMode::fine_grained;
  }
  bool isBlock() const { return plan.mode == PartitionMode::block; }
  bool isStencil() const { return plan.mode == PartitionMode::stencil; }
  bool isCoarse() const { return plan.mode == PartitionMode::coarse; }

protected:
  /// Protected constructor - use create() factory instead
  DbRewriter(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
             DbRewritePlan &plan);

  virtual void transformAcquire(const DbRewriteAcquire &info,
                                DbAllocOp newAlloc, OpBuilder &builder) = 0;
  virtual void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                              OpBuilder &builder) = 0;
  virtual bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                              Value startBlock = nullptr,
                              bool isSingleBlock = false) = 0;

  /// Shared state
  DbAllocOp oldAlloc;
  SmallVector<DbRewriteAcquire> acquires;
  DbRewritePlan plan;

  /// Indexer factory

  static std::unique_ptr<DbIndexerBase>
  createIndexer(const DbRewritePlan &plan, Value startBlock, Value elemOffset,
                Value elemSize, unsigned outerRank, unsigned innerRank,
                ValueRange oldElementSizes, OpBuilder &builder, Location loc,
                Value ownedArg = nullptr, Value leftHaloArg = nullptr,
                Value rightHaloArg = nullptr);

  static std::unique_ptr<DbIndexerBase>
  createElementWiseIndexer(ArrayRef<Value> elemOffsets, unsigned outerRank,
                           unsigned innerRank, ValueRange oldElementSizes);

  static std::unique_ptr<DbIndexerBase>
  createBlockIndexer(ArrayRef<Value> blockSizes, ArrayRef<Value> startBlocks,
                     unsigned outerRank, unsigned innerRank);

  static std::unique_ptr<DbIndexerBase>
  createStencilIndexer(const StencilInfo &info, Value blockSize,
                       Value elemOffset, unsigned outerRank, unsigned innerRank,
                       Value ownedArg, Value leftHaloArg, Value rightHaloArg,
                       OpBuilder &builder, Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
