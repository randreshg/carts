///==========================================================================///
/// File: DbStencilRewriter.h
///
/// Specialized index rewriter for stencil access patterns.
/// Current default for stencil is element-wise; ESD is the planned path.
///
/// Partitioning policy:
/// - Stencil/mixed patterns default to element-wise today.
/// - Planned path (see STENCIL_ESD_IMPLEMENTATION_PLAN.md): ESD uses chunk
///   ownership + slice gets (ARTS_PTR deps) with fixed depc slots.
///
/// Example (row stencil, chunk rows [start, end)):
///   left halo row  = start-1, right halo row = end
///   offsetBytes    = rowIndex * rowBytes
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H

#include "arts/Transforms/Datablock/DbRewriterBase.h"

namespace mlir {
namespace arts {

/// Specialized index rewriter for stencil access patterns.
/// Implements halo-aware clamping and offset adjustment.
///
/// Index Localization Formulas:
///   dbRefIdx = 0 (single chunk with halo)
///   memrefIdx = clamp(globalIdx, 0, totalRows-1) - chunkStart + haloLeft
class DbStencilRewriter : public DbRewriterBase {
  /// Stencil-specific members (kept for future ESD implementation)
  Value baseChunkSize_, startChunk_, haloLeft_, haloRight_, totalRows_;
  Value elemOffset_, elemSize_;
  SmallVector<Value> oldElementSizes_;

public:
  DbStencilRewriter(Value baseChunkSize, Value startChunk, Value haloLeft,
                    Value haloRight, Value totalRows, unsigned outerRank,
                    unsigned innerRank, Value elemOffset, Value elemSize,
                    ValueRange oldElementSizes);

  /// Transform global multi-dimensional indices to local with halo offset.
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// Transform a linearized global index (for flattened 1D memrefs)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  /// Rewrite a DbRefOp and its load/store users
  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// Rebase a list of operations with stencil-aware localization
  void rebaseOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                 ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

private:
  /// Clamp global index to valid array bounds [0, totalRows-1]
  /// Handles boundary workers where stencil would access out-of-bounds
  Value clampToBounds(Value globalIdx, OpBuilder &builder, Location loc);

  /// Compute local index with halo offset
  /// localIdx = clampedGlobalIdx - chunkStart + haloLeft
  Value computeLocalIndex(Value globalIdx, Value chunkStart, OpBuilder &builder,
                          Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
