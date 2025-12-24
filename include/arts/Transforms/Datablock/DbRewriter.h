///==========================================================================///
/// File: DbRewriter.h
///
/// - Apply the selected rewriter (chunked/element-wise/stencil placeholder).
/// - Rewrite DbAcquire/DbRef users to the new allocation.
/// - Clean up old ops once rewrites are complete.
///
/// Dispatch flow (high level):
///   DbPartitioning decides -> DbRewriter constructs -> apply() rewrites.
///
/// Flowchart (dispatch + cleanup):
///
///   DbPartitioning
///        │
///        │  mode + sizes + offsets + chunkSize + (optional stencil info)
///        ▼
///   DbRewriter::apply()
///        │
///        ├─ create new db.alloc (new sizes)
///        ├─ rewrite each db.acquire (offsets/sizes + EDT block args)
///        ├─ rebase EDT users via specialized rewriter:
///        │     ├─ ElementWise → DbElementWiseRewriter
///        │     ├─ Chunked     → DbChunkedRewriter
///        │     └─ Stencil     → DbStencilRewriter (placeholder)
///        ├─ rewrite db_ref users outside EDTs
///        ├─ replace old alloc uses
///        └─ erase old ops (cleanup)
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H

#include "arts/ArtsDialect.h"
#include "arts/Transforms/Datablock/DbRewriterBase.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Rewriter modes for datablock partitioning
///===----------------------------------------------------------------------===///
enum class RewriterMode {
  /// Element-wise: Each element is a separate datablock.
  /// Index transform: outer[globalIdx], inner[0]
  ElementWise,

  /// Chunked: Uniform chunks with div/mod localization.
  /// Index transform: dbRef[globalRow/chunkSize - startChunk],
  ///                  memref[globalRow % chunkSize, ...]
  Chunked,

  /// Stencil: Reserved for future ESD-based stencil localization (not default).
  /// Rewriter: DbStencilRewriter
  Stencil
};

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

///===----------------------------------------------------------------------===///
/// Stencil info for future stencil localization (ESD placeholder)
struct StencilInfo {
  int64_t haloLeft = 0;  /// Left halo size (|minOffset|)
  int64_t haloRight = 0; /// Right halo size (|maxOffset|)
  Value totalRows;       /// Total rows in array (for boundary clamping)

  bool hasHalo() const { return haloLeft > 0 || haloRight > 0; }
};

/// Per-acquire rewrite input (element-space offsets/sizes).
struct DbRewriteAcquire {
  DbAcquireOp acquire;
  Value elemOffset;
  Value elemSize;
};

/// Rewriter plan chosen by DbPartitioning.
struct DbRewritePlan {
  RewriterMode mode;
  Value chunkSize; ///< Valid for Chunked/Stencil; empty for ElementWise
  std::optional<StencilInfo> stencilInfo;

  /// Explicit rank configuration for flexible partitioning.
  /// Specifying outerRank and innerRank directly allows:
  /// - Inner->Outer promotion (increasing outerRank)
  /// - Outer->Inner demotion (decreasing outerRank)
  /// When 0, auto-compute from mode defaults.
  unsigned outerRank = 0;
  unsigned innerRank = 0;

  /// Validate plan consistency.
  /// Returns true if the plan has all required fields for its mode.
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

class DbRewriter {
public:
  /// Constructor: all inputs provided upfront.
  /// Mode and mode-specific parameters are provided by the caller.
  DbRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
             ValueRange newInnerSizes, ArrayRef<DbRewriteAcquire> acquires,
             const DbRewritePlan &plan);

  /// Apply the transformation: create new allocation and rewrite all uses.
  FailureOr<DbAllocOp> apply(OpBuilder &builder);

  /// Get the rewriter mode for this transformation.
  RewriterMode getMode() const { return plan_.mode; }

  /// Convenience accessors
  bool isElementWise() const { return plan_.mode == RewriterMode::ElementWise; }
  bool isChunked() const { return plan_.mode == RewriterMode::Chunked; }
  bool isStencil() const { return plan_.mode == RewriterMode::Stencil; }

  /// Create mode-specific rewriter based on plan.
  /// Returns a rewriter for localization of indices within EDT bodies.
  static std::unique_ptr<DbRewriterBase>
  createRewriter(const DbRewritePlan &plan, Value chunkSize, Value startChunk,
                 Value elemOffset, Value elemSize, unsigned outerRank,
                 unsigned innerRank, ValueRange oldElementSizes,
                 OpBuilder &builder, Location loc);

private:
  DbAllocOp oldAlloc_;
  SmallVector<Value> newOuterSizes_;
  SmallVector<Value> newInnerSizes_;

  /// Partition info per acquire
  SmallVector<DbRewriteAcquire> acquires_;

  /// Provided by the caller
  DbRewritePlan plan_;

  /// Rewrite a single acquire with its partition info
  void rewriteAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                      OpBuilder &builder);

  /// Rewrite a DbRefOp with transformed indices
  void rewriteDbRef(DbRefOp ref, DbAllocOp newAlloc, OpBuilder &builder);

  /// Rebase all users of an acquired EDT block argument to local coordinates
  /// Mode-aware replacement for db::rebaseAllUsersToAcquireView
  /// For chunked mode, startChunk should be passed from rewriteAcquire to avoid
  /// recomputation. If nullptr in chunked mode, falls back to reading from
  /// hints.
  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value startChunk = nullptr);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
