///==========================================================================///
/// File: DbChunkedRewriter.h
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

// Forward declaration
class ArtsCodegen;

///===----------------------------------------------------------------------===///
/// DbChunkedRewriter 
///
/// For chunked allocations:
///   dbRefIdx = (globalRow / chunkSize) - startChunk
///   memrefIdx = globalRow % chunkSize
///===----------------------------------------------------------------------===///
class DbChunkedRewriter {
  Value chunkSize_;  /// Elements per chunk
  Value startChunk_; /// First chunk this partition acquires
  Value elemOffset_; /// Element offset (for stride computation)
  unsigned outerRank_;
  unsigned innerRank_;

public:
  DbChunkedRewriter(Value chunkSize, Value startChunk, Value elemOffset,
                    unsigned outerRank, unsigned innerRank);

  /// Result of localizing indices - defined locally, not from base class
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  /// Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; /// Indices for memref load/store
  };

  /// Transform global multi-dimensional indices to local
  /// For chunked mode: dbRefIdx = global / chunkSize - startChunk
  ///                   memrefIdx = global % chunkSize
  LocalizedIndices localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                            Location loc);

  /// Transform a linearized global index (for flattened 1D memrefs)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      OpBuilder &builder, Location loc);

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

  /// Mode-aware coordinate localization
  SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBCHUNKEDREWRITER_H
