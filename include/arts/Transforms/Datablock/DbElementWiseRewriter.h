///==========================================================================///
/// File: DbElementWiseRewriter.h
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

// Forward declaration
class ArtsCodegen;

///===----------------------------------------------------------------------===///
/// DbElementWiseRewriter
///
/// For element-wise allocations:
///   dbRefIdx = globalRow - elemOffset
///   memrefIdx = remaining dimensions
///
/// For linearized indices, scales offset by stride
///   localLinear = globalLinear - (elemOffset * stride)
///===----------------------------------------------------------------------===///
class DbElementWiseRewriter {
  Value elemOffset_; /// First element this partition owns
  Value elemSize_;   /// Number of elements owned
  unsigned outerRank_;
  unsigned innerRank_;
  SmallVector<Value>
      oldElementSizes_; /// Old allocation's element sizes for stride

public:
  DbElementWiseRewriter(Value elemOffset, Value elemSize, unsigned outerRank,
                        unsigned innerRank, ValueRange oldElementSizes = {});

  /// Result of localizing indices - defined locally, not from base class
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  /// Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; /// Indices for memref load/store
  };

private:
  /// Split indices into outer/db_ref and inner/memref groups.
  LocalizedIndices splitIndices(ValueRange globalIndices, OpBuilder &builder,
                                Location loc);

public:
  /// Transform global multi-dimensional indices to local
  /// For element-wise mode: split indices by outer/inner rank and
  /// subtract elemOffset from the first outer index.
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc);

  /// Localize indices against a fine-grained acquire view.
  LocalizedIndices localizeForFineGrained(ValueRange globalIndices,
                                          ValueRange acquireIndices,
                                          ValueRange acquireOffsets,
                                          OpBuilder &builder, Location loc);

  /// Transform a linearized global index
  /// localLinear = globalLinear - (elemOffset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder, Location loc);

  /// Rewrite a single load/store using db_ref + load/store pattern.
  void rewriteAccessWithDbPattern(Operation *op, Value dbPtr, Type elementType,
                                  ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove);

  /// Rebase a list of operations
  void rebaseOps(ArrayRef<Operation *> ops, Value dbPtr, Type elementType,
                 ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove);

  /// Rewrite all uses of an allocation in the parent region
  void rewriteUsesInParentRegion(Operation *alloc, DbAllocOp dbAlloc,
                                 ArtsCodegen &AC,
                                 llvm::SetVector<Operation *> &opsToRemove);

  /// Rewrite a DbRefOp and its load/store users
  void rewriteDbRefUsers(DbRefOp ref, Value blockArg, Type newElementType,
                         OpBuilder &builder,
                         llvm::SetVector<Operation *> &opsToRemove);

  /// Element-wise localizeCoordinates scales offset by stride
  /// for linearized accesses
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBELEMENTWISEREWRITER_H
