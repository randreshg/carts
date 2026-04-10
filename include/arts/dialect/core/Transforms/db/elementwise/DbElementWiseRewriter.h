///==========================================================================///
/// File: DbElementWiseRewriter.h
///
/// Element-wise rewriter: each element is a separate datablock.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEREWRITER_H
#define ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEREWRITER_H

#include "arts/dialect/core/Transforms/db/DbRewriter.h"

namespace mlir {
namespace arts {

class DbElementWiseRewriter : public DbRewriter {
public:
  DbElementWiseRewriter(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
                        DbRewritePlan &plan)
      : DbRewriter(oldAlloc, acquires, plan) {}

protected:
  void transformAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                        OpBuilder &builder) override;

  void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                      OpBuilder &builder) override;

  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value startBlock = nullptr,
                      bool isSingleChunk = false) override;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_TRANSFORMS_DB_ELEMENTWISE_DBELEMENTWISEREWRITER_H
