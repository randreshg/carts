///==========================================================================///
/// File: DbStencilIndexer.h
///
/// Stencil-aware index localization for ESD (Ephemeral Slice Dependencies).
///
/// 3-Buffer Selection Model:
///   ESD delivers 3 separate buffers: owned, leftHalo, rightHalo
///   At each access, select the correct buffer based on localRow:
///     - localRow < haloLeft: access leftHaloArg
///     - localRow < haloLeft + blockSize: access ownedArg
///     - else: access rightHaloArg
///
/// The Stencil indexer has significantly different logic from Block/ElementWise
/// (3-buffer selection, row loop versioning, halo handling), so it overrides
/// transformDbRefUsers and transformOps entirely rather than using the base
/// class shared implementations.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DB_STENCIL_DBSTENCILINDEXER_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DB_STENCIL_DBSTENCILINDEXER_H

#include "arts/dialect/core/Transforms/db/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Stencil-aware index localizer using 3-buffer conditional selection.
/// Extends block mode with halo regions for neighbor access patterns.
///
/// Uses partitionInfo.offsets for elemOffset and partitionInfo.sizes for
/// blockSize. Additional stencil-specific data (halo sizes, buffer args) are
/// stored separately.
///
/// Overrides transformDbRefUsers and transformOps from the base class with
/// stencil-specific 3-buffer selection logic.
class DbStencilIndexer : public DbIndexerBase {
  Value elemOffset;
  Value haloLeft;     /// Left halo size (number of rows)
  Value haloRight;    /// Right halo size (number of rows)
  Value blockSize;    /// Owned block size (rows per worker)
  Value leftAvail;    /// Semantic availability for left halo
  Value rightAvail;   /// Semantic availability for right halo
  Value ownedArg;     /// Block arg for owned buffer
  Value leftHaloArg;  /// Block arg for left halo (may be null at boundary)
  Value rightHaloArg; /// Block arg for right halo (may be null at boundary)

public:
  /// Constructor with PartitionInfo - uses partitionInfo.offsets[0] as
  /// baseOffset and partitionInfo.sizes[0] as blockSize.
  /// Uses base-offset semantics where localRow = globalRow - baseOffset.
  DbStencilIndexer(const PartitionInfo &info, Value haloLeft, Value haloRight,
                   unsigned outerRank, unsigned innerRank, Value leftAvail,
                   Value rightAvail, Value ownedArg, Value leftHaloArg,
                   Value rightHaloArg);

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  /// Stencil-specific 3-buffer transformDbRefUsers (overrides base).
  void transformDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                           OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  /// Stencil-specific transformOps (delegates DbRefOps to transformDbRefUsers).
  void transformOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DB_STENCIL_DBSTENCILINDEXER_H
