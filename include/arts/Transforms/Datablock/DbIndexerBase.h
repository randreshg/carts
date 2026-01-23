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
/// - DbBlockIndexer: div/mod localization for block allocation
/// - DbElementWiseIndexer: direct element coordinate mapping
/// - DbStencilIndexer: halo-aware clamping and offset
///
/// All indexers hold PartitionInfo as the canonical source of partition data:
/// - DbElementWiseIndexer uses partitionInfo.indices (element COORDINATES)
/// - DbBlockIndexer uses partitionInfo.offsets/sizes (range start/size)
/// - DbStencilIndexer uses partitionInfo.offsets/sizes + halo info
class DbIndexerBase {
protected:
  /// Canonical source of partition semantics. Indexers access the fields
  /// appropriate for their mode:
  /// - fine_grained: use partitionInfo.indices directly as db_ref indices
  /// - block/stencil: use partitionInfo.offsets/sizes for div/mod formulas
  PartitionInfo partitionInfo;
  unsigned outerRank, innerRank;

public:
  /// Constructor with PartitionInfo - the canonical way to create indexers.
  DbIndexerBase(const PartitionInfo &info, unsigned outerRank,
                unsigned innerRank)
      : partitionInfo(info), outerRank(outerRank), innerRank(innerRank) {}

  /// Accessors for partition info
  const PartitionInfo &getPartitionInfo() const { return partitionInfo; }
  PartitionMode getMode() const { return partitionInfo.mode; }

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

  unsigned getOuterRank() const { return outerRank; }
  unsigned getInnerRank() const { return innerRank; }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBINDEXERBASE_H
