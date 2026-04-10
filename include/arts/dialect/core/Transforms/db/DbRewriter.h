///==========================================================================///
/// File: DbRewriter.h
///
/// Abstract base class for datablock allocation transformation.
///
/// Rewriter responsibilities are intentionally narrow:
///   - apply an already chosen layout
///   - rewrite acquires/db_refs to match that layout
///   - delegate index localization to mode-specific indexers
///
/// Rewriters do not re-run heuristics or negotiate legality. Those decisions
/// must be finished before a DbRewritePlan reaches this layer.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DB_DBREWRITER_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DB_DBREWRITER_H

#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/graphs/db/DbAccessPattern.h"
#include "arts/dialect/core/Transforms/db/DbIndexerBase.h"
#include "arts/utils/PartitionPredicates.h"
#include "arts/utils/Utils.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Partition mode utilities
///===----------------------------------------------------------------------===///

/// Check if mode requires block size parameter
inline bool requiresBlockSize(PartitionMode mode) {
  return usesBlockLayout(mode);
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

///===----------------------------------------------------------------------===///
/// Stencil info for ESD halo-based localization
///===----------------------------------------------------------------------===///
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
};

/// Rewriter plan chosen by DbPartitioning.
struct DbRewritePlan {
  PartitionMode mode = PartitionMode::fine_grained;
  std::optional<StencilInfo> stencilInfo;

  /// N-D block sizes (one per partitioned dimension)
  SmallVector<Value> blockSizes;
  /// Original dimension indices for each partitioned dimension
  SmallVector<unsigned> partitionedDims;

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
  unsigned numPartitionedDims() const {
    return !partitionedDims.empty() ? partitionedDims.size()
                                    : blockSizes.size();
  }

  bool isCoarse() const { return !requiresWorkerBoundsPlanning(mode); }
  bool isElementWise() const { return arts::usesElementLayout(mode); }
  bool usesBlockedLayout() const { return arts::usesBlockLayout(mode); }
  bool isStencil() const { return arts::supportsHaloExtension(mode); }
  bool isBlock() const { return usesBlockedLayout() && !isStencil(); }

  bool isValid() const {
    if (isStencil())
      return stencilInfo.has_value();
    if (usesBlockedLayout())
      return !blockSizes.empty();
    return true;
  }

  /// Default constructor
  DbRewritePlan() = default;
  explicit DbRewritePlan(PartitionMode m) : mode(m) {}
};

///===----------------------------------------------------------------------===///
/// DbRewriter - Abstract base for allocation transformation
///===----------------------------------------------------------------------===///
class DbRewriter {
public:
  virtual ~DbRewriter() = default;

  static std::unique_ptr<DbRewriter> create(DbAllocOp oldAlloc,
                                            ArrayRef<DbRewriteAcquire> acquires,
                                            DbRewritePlan &plan);
  FailureOr<DbAllocOp> apply(OpBuilder &builder);

  /// Accessors
  PartitionMode getMode() const { return plan.mode; }
  bool isElementWise() const { return plan.isElementWise(); }
  bool isBlock() const { return plan.isBlock(); }
  bool isStencil() const { return plan.isStencil(); }
  bool isCoarse() const { return plan.isCoarse(); }

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
  createElementWiseIndexer(ArrayRef<Value> elemOffsets, unsigned outerRank,
                           unsigned innerRank, ValueRange oldElementSizes);

  static std::unique_ptr<DbIndexerBase> createBlockIndexer(
      ArrayRef<Value> blockSizes, ArrayRef<Value> startBlocks,
      unsigned outerRank, unsigned innerRank,
      ArrayRef<unsigned> partitionedDims, bool allocSingleBlock = false,
      bool acquireSingleBlock = false, Value dominantZero = Value());

  static std::unique_ptr<DbIndexerBase>
  createStencilIndexer(const StencilInfo &info, Value blockSize,
                       Value elemOffset, unsigned outerRank, unsigned innerRank,
                       ArrayRef<unsigned> partitionedDims, Value ownedArg,
                       Value leftHaloArg, Value rightHaloArg,
                       OpBuilder &builder, Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DB_DBREWRITER_H
