///==========================================================================///
/// File: CodirToArtsBlockLocalRewrite.h
///
/// Application of planned block-local access rewrites to EDT bodies.
///==========================================================================///
#ifndef CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALREWRITE_H
#define CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALREWRITE_H

#include "CodirToArtsBlockLocalIndex.h"

namespace {

static inline LogicalResult rewritePlannedBlockLocalAccesses(
    arts::EdtOp task, ArrayRef<PlannedBlockLocalAccessRewrite> rewrites,
    const DenseMap<Value, Value> *sourceByBlockArgument = nullptr) {
  if (rewrites.empty())
    return success();

  auto rewriteIndices = [&](Operation *op, MutableOperandRange memrefOperand,
                            MutableOperandRange indices) -> WalkResult {
    Value memref = memrefOperand[0].get();
    SmallVector<const PlannedBlockLocalAccessRewrite *, 4> matching;
    for (const PlannedBlockLocalAccessRewrite &rewrite : rewrites)
      if (memref == rewrite.localMemref)
        matching.push_back(&rewrite);
    if (matching.empty())
      return WalkResult::advance();

    bool hasGrouped = llvm::any_of(
        matching, [](const PlannedBlockLocalAccessRewrite *rewrite) {
          return rewrite->grouped;
        });
    if (hasGrouped) {
      Value sourcePtr;
      unsigned sourceRank = 0;
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (!rewrite->grouped || !rewrite->groupedSourcePtr ||
            rewrite->blockSize <= 0 || rewrite->groupBlockCount <= 0) {
          op->emitError("grouped planned block-local access requires "
                        "block-window facts for every owner dimension");
          return WalkResult::interrupt();
        }
        if (!sourcePtr)
          sourcePtr = rewrite->groupedSourcePtr;
        if (sourcePtr != rewrite->groupedSourcePtr) {
          op->emitError("grouped planned block-local access mixes dependency "
                        "sources");
          return WalkResult::interrupt();
        }
        sourceRank = std::max<unsigned>(sourceRank, rewrite->ownerSlot + 1);
      }
      if (auto sourceType = dyn_cast<MemRefType>(sourcePtr.getType()))
        sourceRank = std::max<unsigned>(sourceRank, sourceType.getRank());
      if (sourceRank == 0)
        sourceRank = 1;

      OpBuilder builder(op);
      SmallVector<Value, 4> dbRefIndices(
          sourceRank, createZeroIndex(builder, op->getLoc()));
      for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
        if (indices.size() <= rewrite->ownerDim ||
            rewrite->ownerSlot >= dbRefIndices.size()) {
          op->emitError("grouped planned block-local access has malformed "
                        "owner-dimension facts");
          return WalkResult::interrupt();
        }
        Value relativeBlock;
        FailureOr<Value> localIndex = materializeGroupedBlockLocalIndex(
            builder, op->getLoc(), indices[rewrite->ownerDim].get(),
            rewrite->ownerBase, rewrite->localOrigin, rewrite->lowerHalo,
            rewrite->upperHalo, rewrite->blockSize, rewrite->groupBlockCount,
            rewrite->ownerWindowExtent, rewrite->sourceDimExtent, relativeBlock,
            rewrite->allowFullWindowAccess, rewrite->requireOwnerWindowProof,
            sourceByBlockArgument);
        if (failed(localIndex)) {
          op->emitError("grouped planned block-local access for owner dim ")
              << rewrite->ownerDim << " does not stay within the block window";
          return WalkResult::interrupt();
        }
        dbRefIndices[rewrite->ownerSlot] = relativeBlock;
        indices[rewrite->ownerDim].set(*localIndex);
      }
      Value selectedPayload =
          arts::DbRefOp::create(builder, op->getLoc(), sourcePtr, dbRefIndices);
      memrefOperand.assign(ValueRange{selectedPayload});
      return WalkResult::advance();
    }

    for (const PlannedBlockLocalAccessRewrite *rewrite : matching) {
      if (indices.size() <= rewrite->ownerDim) {
        op->emitError("planned block-local access is missing the owner "
                      "dimension index");
        return WalkResult::interrupt();
      }

      OpBuilder builder(op);
      FailureOr<Value> localIndex =
          rewrite->rankExpandedGridAccess
              ? materializeRankExpandedGridLocalIndex(
                    builder, op->getLoc(), indices[rewrite->ownerDim].get(),
                    rewrite->ownerBase, rewrite->ownerDomainBase,
                    rewrite->rankExpandedTileExtent, sourceByBlockArgument)
              : materializeBlockLocalIndex(
                    builder, op->getLoc(), indices[rewrite->ownerDim].get(),
                    rewrite->ownerBase, rewrite->localOrigin,
                    rewrite->lowerHalo);
      if (failed(localIndex)) {
        op->emitError("planned block-local access does not stay within the "
                      "owner slice");
        return WalkResult::interrupt();
      }
      indices[rewrite->ownerDim].set(*localIndex);
    }
    return WalkResult::advance();
  };

  Block &body = task.getBody().front();
  WalkResult result = body.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op))
      return rewriteIndices(op, load.getMemrefMutable(),
                            load.getIndicesMutable());
    if (auto store = dyn_cast<memref::StoreOp>(op))
      return rewriteIndices(op, store.getMemrefMutable(),
                            store.getIndicesMutable());
    return WalkResult::advance();
  });

  return result.wasInterrupted() ? failure() : success();
}

} // namespace

#endif // CARTS_DIALECT_CODIR_CONVERSION_CODIRTOARTS_BLOCKLOCALREWRITE_H
