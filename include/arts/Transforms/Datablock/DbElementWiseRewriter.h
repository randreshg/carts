///==========================================================================///
/// File: DbElementWiseRewriter.h
///
/// Implementation for element-wise (row-per-task) mode index localization.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// DbElementWiseRewriter - Implementation for element-wise (row-per-task) mode
///
/// For element-wise allocations:
///   dbRefIdx = globalRow - elemOffset
///   memrefIdx = remaining dimensions
///
/// For linearized indices, scales offset by stride!
///   localLinear = globalLinear - (elemOffset * stride)
///===----------------------------------------------------------------------===///
class DbElementWiseRewriter : public DbRewriter {
  Value elemOffset_; /// First element this partition owns
  Value elemSize_;   /// Number of elements owned
  unsigned outerRank_;
  unsigned innerRank_;
  SmallVector<Value>
      oldElementSizes_; /// Old allocation's element sizes for stride

public:
  DbElementWiseRewriter(Value elemOffset, Value elemSize, unsigned outerRank,
                      unsigned innerRank, ValueRange oldElementSizes = {});

  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc) override;

  /// scales offset by stride before subtracting!
  /// localLinear = globalLinear - (elemOffset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder,
                                      Location loc) override;

  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove) override;

  /// Element-wise localizeCoordinates scales offset by stride
  /// for linearized accesses
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

#endif // ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
