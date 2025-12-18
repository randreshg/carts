///==========================================================================///
/// File: DbChunkedRewriter.h
///
/// Implementation for chunked (div/mod) mode index localization.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// DbChunkedRewriter - Implementation for chunked (div/mod) mode
///
/// For chunked allocations:
///   dbRefIdx = (globalRow / chunkSize) - startChunk
///   memrefIdx = globalRow % chunkSize
///===----------------------------------------------------------------------===///
class DbChunkedRewriter : public DbRewriter {
  Value chunkSize_;  /// Elements per chunk
  Value startChunk_; /// First chunk this partition acquires
  Value elemOffset_; /// Element offset (for stride computation)
  unsigned outerRank_;
  unsigned innerRank_;

public:
  DbChunkedRewriter(Value chunkSize, Value startChunk, Value elemOffset,
                  unsigned outerRank, unsigned innerRank);

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc) override;

  bool
  rebaseToAcquireViewImpl(Operation *op, DbAcquireOp acquire, Value dbPtr,
                          Type elementType, ArtsCodegen &AC,
                          llvm::SetVector<Operation *> &opsToRemove) override;

  bool rebaseAllUsersToAcquireViewImpl(DbAcquireOp acquire,
                                       ArtsCodegen &AC) override;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
