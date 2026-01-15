///==========================================================================///
/// File: DbElementWiseRewriter.h
///
/// Element-wise rewriter: each element is a separate datablock.
/// Simplest mode - uses direct subtraction for index localization.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H

#include "arts/Transforms/Datablock/DbRewriter.h"

namespace mlir {
namespace arts {

class DbElementWiseRewriter : public DbRewriter {
public:
  DbElementWiseRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                        ValueRange newInnerSizes,
                        ArrayRef<DbRewriteAcquire> acquires,
                        const DbRewritePlan &plan)
      : DbRewriter(oldAlloc, newOuterSizes, newInnerSizes, acquires, plan) {}

protected:
  void transformAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                        OpBuilder &builder) override;

  void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                      OpBuilder &builder) override;

  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value startChunk = nullptr) override;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
