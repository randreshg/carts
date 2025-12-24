///==========================================================================///
/// DbRewriterBase.h - Abstract base class for index rewriters
///
/// This file defines the common interface and shared utilities for all
/// datablock index rewriters (Chunked, ElementWise, Stencil).
///
/// Key abstractions:
/// - LocalizedIndices: Result struct for index localization
/// - getIndicesFromOp(): Helper to extract indices from load/store/ref ops
/// - DbRewriterBase: Abstract base class with pure virtual localize methods
///
/// Design note: Chunked and ElementWise use fundamentally different arithmetic
/// (div/mod vs subtraction), so they remain separate classes. The base class
/// extracts common patterns without forcing artificial unification.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBREWRITERBASE_H
#define ARTS_TRANSFORMS_DATABLOCK_DBREWRITERBASE_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// Shared result structure for index localization.
/// Used by all rewriter modes to return the transformed indices.
struct LocalizedIndices {
  SmallVector<Value> dbRefIndices;  ///< Indices for arts.db_ref operation
  SmallVector<Value> memrefIndices; ///< Indices for memref load/store
};

/// Shared helper - extract indices from load/store/ref operations.
/// This function was duplicated across all three rewriters.
inline ValueRange getIndicesFromOp(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getIndices();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getIndices();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getIndices();
  return {};
}

/// Forward declaration
class ArtsCodegen;

/// Abstract base class for datablock index rewriters.
///
/// Each derived class implements mode-specific localization:
/// - DbChunkedRewriter: div/mod localization for chunked allocation
/// - DbElementWiseRewriter: subtraction-based offset adjustment
/// - DbStencilRewriter: halo-aware clamping and offset
class DbRewriterBase {
protected:
  unsigned outerRank_; ///< Dimensions for DB grid indexing
  unsigned innerRank_; ///< Dimensions for element indexing within DB

public:
  DbRewriterBase(unsigned outerRank, unsigned innerRank)
      : outerRank_(outerRank), innerRank_(innerRank) {}

  virtual ~DbRewriterBase() = default;

  //===--------------------------------------------------------------------===//
  // Pure virtual methods - mode-specific localization
  //===--------------------------------------------------------------------===//

  /// Transform global multi-dimensional indices to local coordinates.
  /// Each rewriter mode implements different localization logic.
  virtual LocalizedIndices localize(ArrayRef<Value> globalIndices,
                                    OpBuilder &builder, Location loc) = 0;

  /// Transform a linearized global index for flattened 1D memrefs.
  virtual LocalizedIndices localizeLinearized(Value globalLinearIndex,
                                              Value stride, OpBuilder &builder,
                                              Location loc) = 0;

  /// Rewrite a DbRefOp and its load/store users to use local coordinates.
  /// Each rewriter has different stride detection logic:
  /// - Chunked: derives stride from MemRefType
  /// - ElementWise: uses stored oldElementSizes_
  /// - Stencil: uses halo-aware bounds
  virtual void rewriteDbRefUsers(DbRefOp ref, Value blockArg,
                                 Type newElementType, OpBuilder &builder,
                                 llvm::SetVector<Operation *> &opsToRemove) = 0;

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  unsigned getOuterRank() const { return outerRank_; }
  unsigned getInnerRank() const { return innerRank_; }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBREWRITERBASE_H
