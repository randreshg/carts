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
/// Rewriter modes for datablock partitioning
///===----------------------------------------------------------------------===//
enum class RewriterMode { ElementWise, Chunked, Stencil };

/// Check if mode requires chunk size parameter
inline bool requiresChunkSize(RewriterMode mode) {
  return mode == RewriterMode::Chunked || mode == RewriterMode::Stencil;
}

/// Get string name for mode (for debugging/logging)
inline StringRef getRewriterModeName(RewriterMode mode) {
  switch (mode) {
  case RewriterMode::ElementWise:
    return "ElementWise";
  case RewriterMode::Chunked:
    return "Chunked";
  case RewriterMode::Stencil:
    return "Stencil";
  }
  return "Unknown";
}

/// Convert RewriterMode to PartitionMode
inline PartitionMode toPartitionMode(RewriterMode mode) {
  switch (mode) {
  case RewriterMode::Chunked:
    return PartitionMode::chunked;
  case RewriterMode::Stencil:
    return PartitionMode::chunked;
  case RewriterMode::ElementWise:
    return PartitionMode::fine_grained;
  }
  return PartitionMode::coarse;
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
struct DbRewriteAcquire {
  DbAcquireOp acquire;
  Value elemOffset;
  Value elemSize;
  bool isFullRange = false;
  bool skipRebase = false;
};

/// Forward declaration for PartitioningDecision
struct PartitioningDecision;

/// Rewriter plan chosen by DbPartitioning.
struct DbRewritePlan {
  RewriterMode mode = RewriterMode::ElementWise;
  Value chunkSize;
  std::optional<StencilInfo> stencilInfo;
  unsigned outerRank = 0;
  unsigned innerRank = 0;

  /// Mixed mode support
  bool isMixedMode = false;
  Value numChunks;

  /// Default constructor
  DbRewritePlan() = default;
  explicit DbRewritePlan(const PartitioningDecision &decision);

  bool isValid() const {
    switch (mode) {
    case RewriterMode::Stencil:
      return stencilInfo.has_value();
    case RewriterMode::Chunked:
      return static_cast<bool>(chunkSize);
    case RewriterMode::ElementWise:
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
  RewriterMode getMode() const { return plan.mode; }
  bool isElementWise() const { return plan.mode == RewriterMode::ElementWise; }
  bool isChunked() const { return plan.mode == RewriterMode::Chunked; }
  bool isStencil() const { return plan.mode == RewriterMode::Stencil; }

protected:
  /// Protected constructor - use create() factory instead
  DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
             ValueRange newInnerSizes, ArrayRef<DbRewriteAcquire> acquires,
             const DbRewritePlan &plan);

  ///===--------------------------------------------------------------------===///
  /// Virtual hooks
  ///===--------------------------------------------------------------------===///

  virtual void transformAcquire(const DbRewriteAcquire &info,
                                DbAllocOp newAlloc, OpBuilder &builder) = 0;
  virtual void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                              OpBuilder &builder) = 0;
  virtual bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                              Value startChunk = nullptr) = 0;

  ///===--------------------------------------------------------------------===///
  /// Shared state
  ///===--------------------------------------------------------------------===///

  DbAllocOp oldAlloc;
  SmallVector<Value> newOuterSizes, newInnerSizes;
  SmallVector<DbRewriteAcquire> acquires;
  DbRewritePlan plan;

  ///===--------------------------------------------------------------------===///
  /// Indexer factory - used by subclasses for index localization
  ///===--------------------------------------------------------------------===///

  static std::unique_ptr<DbIndexerBase>
  createIndexer(const DbRewritePlan &plan, Value startChunk, Value elemOffset,
                Value elemSize, unsigned outerRank, unsigned innerRank,
                ValueRange oldElementSizes, OpBuilder &builder, Location loc,
                Value ownedArg = nullptr, Value leftHaloArg = nullptr,
                Value rightHaloArg = nullptr);

  static std::unique_ptr<DbIndexerBase>
  createElementWiseIndexer(Value elemOffset, Value elemSize, unsigned outerRank,
                           unsigned innerRank, ValueRange oldElementSizes);

  static std::unique_ptr<DbIndexerBase>
  createChunkedIndexer(Value chunkSize, Value startChunk, Value elemOffset,
                       unsigned outerRank, unsigned innerRank);

  static std::unique_ptr<DbIndexerBase>
  createStencilIndexer(const StencilInfo &info, Value chunkSize,
                       Value elemOffset, unsigned outerRank, unsigned innerRank,
                       Value ownedArg, Value leftHaloArg, Value rightHaloArg,
                       OpBuilder &builder, Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
