///==========================================================================///
/// File: DbBlockRewriter.h
///
/// Block rewriter: uniform blocks with div/mod localization.
/// Multiple physical blocks may be acquired if partition spans block
/// boundaries.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBBLOCKREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBBLOCKREWRITER_H

#include "arts/Transforms/Datablock/DbRewriter.h"

namespace mlir {
namespace arts {

class DbBlockRewriter : public DbRewriter {
public:
  DbBlockRewriter(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
                  DbRewritePlan &plan)
      : DbRewriter(oldAlloc, acquires, plan) {}

protected:
  void transformAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                        OpBuilder &builder) override;

  void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                      OpBuilder &builder) override;

  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value startBlock = nullptr,
                      bool isSingleBlock = false) override;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBBLOCKREWRITER_H
