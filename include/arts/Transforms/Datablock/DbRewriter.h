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

/// Per-acquire rewrite input (element-space offsets/sizes).
/// Supports multi-dimensional fine-grained partitioning (e.g., A[i][j]).
struct DbRewriteAcquire {
  DbAcquireOp acquire;
  /// Multi-dimensional element offsets (e.g., [%i, %j] for 2D fine-grained)
  SmallVector<Value> elemOffsets;
  /// Multi-dimensional element sizes (e.g., [1, 1] for 2D fine-grained)
  SmallVector<Value> elemSizes;
  bool isFullRange = false;
  bool skipRebase = false;

  /// Legacy accessors for 1D backward compatibility
  Value getElemOffset() const {
    return elemOffsets.empty() ? Value() : elemOffsets.front();
  }
  Value getElemSize() const {
    return elemSizes.empty() ? Value() : elemSizes.front();
  }
};

/// Forward declaration for PartitioningDecision
struct PartitioningDecision;

/// Rewriter plan chosen by DbPartitioning.
struct DbRewritePlan {
  PartitionMode mode = PartitionMode::fine_grained;
  Value blockSize; /// Legacy: single block size for 1D partitioning
  SmallVector<Value> blockSizes; /// N-D: block size per partitioned dimension
  std::optional<StencilInfo> stencilInfo;
  unsigned outerRank = 0;
  unsigned innerRank = 0;

  /// Mixed mode support
  bool isMixedMode = false;
  Value numBlocks;
  SmallVector<Value> numBlocksPerDim; /// N-D: block count per dimension

  /// Default constructor
  DbRewritePlan() = default;
  explicit DbRewritePlan(const PartitioningDecision &decision);

  /// Get block size for dimension d (falls back to blockSize for 1D)
  Value getBlockSize(unsigned d = 0) const {
    if (d < blockSizes.size())
      return blockSizes[d];
    return blockSize;
  }

  /// Number of partitioned dimensions
  unsigned numPartitionedDims() const {
    return blockSizes.empty() ? (blockSize ? 1 : 0) : blockSizes.size();
  }

  bool isValid() const {
    switch (mode) {
    case PartitionMode::stencil:
      return stencilInfo.has_value();
    case PartitionMode::block:
      return static_cast<bool>(blockSize) || !blockSizes.empty();
    case PartitionMode::fine_grained:
    case PartitionMode::coarse:
      return true;
    }
    return false;
  }
};

///===----------------------------------------------------------------------===//
/// DbRewriter - Abstract base for allocation transformation
///===----------------------------------------------------------------------===//
class DbRewriter {
public:
  virtual ~DbRewriter() = default;

  /// Factory method - creates the appropriate subclass based on plan.mode
  static std::unique_ptr<DbRewriter>
  create(DbAllocOp oldAlloc, ValueRange newOuterSizes, ValueRange newInnerSizes,
         ArrayRef<DbRewriteAcquire> acquires, const DbRewritePlan &plan);

  /// Main entry point - shared workflow (template method pattern)
  /// Non-virtual: defines the transformation steps, calls virtual hooks.
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
  DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
             ValueRange newInnerSizes, ArrayRef<DbRewriteAcquire> acquires,
             const DbRewritePlan &plan);

  ///===--------------------------------------------------------------------===///
  /// Virtual hooks
  virtual void transformAcquire(const DbRewriteAcquire &info,
                                DbAllocOp newAlloc, OpBuilder &builder) = 0;
  virtual void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                              OpBuilder &builder) = 0;
  virtual bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                              Value startBlock = nullptr,
                              bool isSingleBlock = false) = 0;

  /// Shared state
  DbAllocOp oldAlloc;
  SmallVector<Value> newOuterSizes, newInnerSizes;
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
