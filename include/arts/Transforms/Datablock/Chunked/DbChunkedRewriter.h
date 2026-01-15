///==========================================================================///
/// File: DbChunkedRewriter.h
///
/// Chunked rewriter: uniform chunks with div/mod localization.
/// Multiple physical chunks may be acquired if partition spans chunk
/// boundaries.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H

#include "arts/Transforms/Datablock/DbRewriter.h"

namespace mlir {
namespace arts {

class DbChunkedRewriter : public DbRewriter {
public:
  DbChunkedRewriter(DbAllocOp oldAlloc, ValueRange newOuterSizes,
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

#endif // ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
