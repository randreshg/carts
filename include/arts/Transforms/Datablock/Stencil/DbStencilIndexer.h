///==========================================================================///
/// File: DbStencilIndexer.h
///
/// Stencil-aware index localization for ESD (Ephemeral Slice Dependencies).
///
/// 3-Buffer Selection Model:
///   ESD delivers 3 separate buffers: owned, leftHalo, rightHalo
///   At each access, select the correct buffer based on localRow:
///     - localRow < haloLeft: access leftHaloArg
///     - localRow < haloLeft + chunkSize: access ownedArg
///     - else: access rightHaloArg
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBSTENCILINDEXER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBSTENCILINDEXER_H

#include "arts/Transforms/Datablock/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Stencil-aware index localizer using 3-buffer conditional selection.
class DbStencilIndexer : public DbIndexerBase {
  Value elemOffset_;
  Value haloLeft_;     /// Left halo size (number of rows)
  Value haloRight_;    /// Right halo size (number of rows)
  Value chunkSize_;    /// Owned chunk size (rows per worker)
  Value ownedArg_;     /// Block arg for owned buffer
  Value leftHaloArg_;  /// Block arg for left halo (may be null at boundary)
  Value rightHaloArg_; /// Block arg for right halo (may be null at boundary)

public:
  DbStencilIndexer(Value haloLeft, Value haloRight, Value chunkSize,
                   unsigned outerRank, unsigned innerRank, Value elemOffset,
                   Value ownedArg, Value leftHaloArg, Value rightHaloArg);

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  void transformDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                           OpBuilder &builder,
                           llvm::SetVector<Operation *> &opsToRemove) override;

  void transformOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBSTENCILINDEXER_H
