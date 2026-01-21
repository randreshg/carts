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
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBSTENCILINDEXER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBSTENCILINDEXER_H

#include "arts/Transforms/Datablock/DbIndexerBase.h"

namespace mlir {
namespace arts {

/// Stencil-aware index localizer using 3-buffer conditional selection.
class DbStencilIndexer : public DbIndexerBase {
  Value elemOffset;
  Value haloLeft;     /// Left halo size (number of rows)
  Value haloRight;    /// Right halo size (number of rows)
  Value blockSize;    /// Owned block size (rows per worker)
  Value ownedArg;     /// Block arg for owned buffer
  Value leftHaloArg;  /// Block arg for left halo (may be null at boundary)
  Value rightHaloArg; /// Block arg for right halo (may be null at boundary)

public:
  DbStencilIndexer(Value haloLeft, Value haloRight, Value blockSize,
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
