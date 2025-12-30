///==========================================================================///
/// File: DbStencilRewriter.h
///
/// Stencil-aware index localization for ESD (Ephemeral Slice Dependencies).
///
/// Index Localization Formula:
///   dbRefIdx  = 0           (single extended chunk per worker)
///   memrefIdx = globalIdx - elemOffset  (with bounds clamping)
///
/// The elemOffset accounts for halo extension so that stencil neighbor
/// accesses (globalIdx-1, globalIdx+1) map to valid local indices.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H

#include "arts/Transforms/Datablock/DbRewriterBase.h"

namespace mlir {
namespace arts {

/// Stencil-aware index rewriter using subtraction-based localization.
class DbStencilRewriter : public DbRewriterBase {
  Value elemOffset_; ///< Extended offset (accounts for halo)
  Value elemSize_;   ///< Extended size (chunk + halos)

public:
  /// Constructor. Only elemOffset and elemSize are used for localization;
  /// other parameters are accepted for API compatibility.
  DbStencilRewriter(Value chunkSize, Value startChunk, Value haloLeft,
                    Value haloRight, Value totalRows, unsigned outerRank,
                    unsigned innerRank, Value elemOffset, Value elemSize,
                    ValueRange oldElementSizes);

  /// Transform global indices to local: localIdx = globalIdx - elemOffset_
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// Transform linearized global index (for flattened memrefs)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  /// Rewrite DbRefOp users with localized indices
  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// Rebase operations with stencil-aware localization
  void rebaseOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                 ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
