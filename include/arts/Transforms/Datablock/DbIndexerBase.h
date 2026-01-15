///==========================================================================///
/// File: DbIndexerBase.h
///
/// Abstract base class for index localizers
///
/// This file defines the common interface and shared utilities for all
/// datablock index localizers (Chunked, ElementWise, Stencil).
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBINDEXERBASE_H
#define ARTS_TRANSFORMS_DATABLOCK_DBINDEXERBASE_H

#include "arts/ArtsDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// Shared result structure for index localization.
/// Used by all indexer modes to return the transformed indices.
struct LocalizedIndices {
  SmallVector<Value> dbRefIndices;
  SmallVector<Value> memrefIndices;
};

/// Shared helper - extract indices from load/store/ref operations.
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

/// Abstract base class for datablock index localizers.
///
/// Each derived class implements mode-specific localization:
/// - DbChunkedIndexer: div/mod localization for chunked allocation
/// - DbElementWiseIndexer: subtraction-based offset adjustment
/// - DbStencilIndexer: halo-aware clamping and offset
class DbIndexerBase {
protected:
  unsigned outerRank_;
  unsigned innerRank_;

public:
  DbIndexerBase(unsigned outerRank, unsigned innerRank)
      : outerRank_(outerRank), innerRank_(innerRank) {}

  virtual ~DbIndexerBase() = default;

  /// Transform global multi-dimensional indices to local coordinates.
  virtual LocalizedIndices localize(ArrayRef<Value> globalIndices,
                                    OpBuilder &builder, Location loc) = 0;

  /// Transform a linearized global index for flattened 1D memrefs.
  virtual LocalizedIndices localizeLinearized(Value globalLinearIndex,
                                              Value stride, OpBuilder &builder,
                                              Location loc) = 0;

  /// Transform a DbRefOp and its load/store users to use local coordinates.
  virtual void
  transformDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                      OpBuilder &builder,
                      llvm::SetVector<Operation *> &opsToRemove) = 0;

  unsigned getOuterRank() const { return outerRank_; }
  unsigned getInnerRank() const { return innerRank_; }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBINDEXERBASE_H
