///==========================================================================///
/// File: DbStencilRewriter.h
///
/// Stencil rewriter: ESD-based stencil with 3-buffer halo.
/// Creates owned chunk + left/right halo acquires for boundary data.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H

#include "arts/Transforms/Db/DbRewriter.h"

namespace mlir {
namespace arts {

class DbStencilRewriter : public DbRewriter {
public:
  DbStencilRewriter(DbAllocOp oldAlloc, ArrayRef<DbRewriteAcquire> acquires,
                    DbRewritePlan &plan)
      : DbRewriter(oldAlloc, acquires, plan) {}

protected:
  void transformAcquire(const DbRewriteAcquire &info, DbAllocOp newAlloc,
                        OpBuilder &builder) override;

  void transformDbRef(DbRefOp ref, DbAllocOp newAlloc,
                      OpBuilder &builder) override;

  bool rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                      Value baseOffset = nullptr,
                      bool isSingleChunk = false) override;

private:
  void transformAcquireAsBlock(const DbRewriteAcquire &info,
                               DbAcquireOp acquire, DbAllocOp newAlloc,
                               OpBuilder &builder);
  bool rebaseEdtUsersAsBlock(DbAcquireOp acquire, OpBuilder &builder);

  /// Add a halo acquire as a new EDT dependency.
  void addHaloAcquireToEdt(DbAcquireOp originalAcq, DbAcquireOp haloAcq,
                           OpBuilder &builder);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBSTENCILREWRITER_H
