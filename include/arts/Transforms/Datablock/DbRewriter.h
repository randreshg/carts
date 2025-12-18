///==========================================================================///
/// File: DbRewriter.h
///
/// Abstract interface for index localization - Strategy pattern.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "llvm/ADT/SetVector.h"
#include <memory>

namespace mlir {
namespace arts {

// Forward declarations
class ArtsCodegen;
class DbChunkedRewriter;
class DbElementWiseRewriter;

///===----------------------------------------------------------------------===///
/// DbRewriter - Abstract interface for index localization
///
/// Strategy pattern: separate implementations for Chunked vs ElementWise.
/// ALL code paths use this interface, ensuring consistent behavior.
///
/// Key benefits over scattered if/else:
/// - Mode encapsulated: caller doesn't know/care if chunked or element-wise
/// - Forces all code through one path: no way to skip stride scaling
/// - Debug-friendly: LLVM_DEBUG in one place covers all uses
/// - N-dimensional support: each implementation handles any rank
///===----------------------------------------------------------------------===///
class DbRewriter {
public:
  virtual ~DbRewriter() = default;

  //===--------------------------------------------------------------------===//
  /// Core Index Transformation API
  //===--------------------------------------------------------------------===//

  /// Result of localizing indices
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  /// Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; /// Indices for memref load/store
  };

  /// Transform global multi-dimensional indices to local
  /// INPUT: globalIndices in original allocation's coordinate space
  /// OUTPUT: {dbRefIndices, memrefIndices} for the promoted structure
  virtual LocalizedIndices localize(ArrayRef<Value> globalIndices,
                                    OpBuilder &builder, Location loc) = 0;

  /// Transform a linearized global index (for flattened 1D memrefs)
  /// Handles stride scaling correctly for each mode
  virtual LocalizedIndices localizeLinearized(Value globalLinearIndex,
                                              Value stride, OpBuilder &builder,
                                              Location loc) = 0;

  /// Rewrite a DbRefOp and its load/store users
  virtual void rewriteDbRefUsers(DbRefOp ref, Value blockArg,
                                 Type newElementType, OpBuilder &builder,
                                 llvm::SetVector<Operation *> &opsToRemove) = 0;

  /// Mode-aware coordinate localization
  virtual SmallVector<Value>
  localizeCoordinates(ArrayRef<Value> globalIndices,
                      ArrayRef<Value> sliceOffsets, unsigned numIndexedDims,
                      Type elementType, OpBuilder &builder, Location loc) = 0;

  /// Mode-aware rebase for a single operation
  virtual bool
  rebaseToAcquireViewImpl(Operation *op, DbAcquireOp acquire, Value dbPtr,
                          Type elementType, ArtsCodegen &AC,
                          llvm::SetVector<Operation *> &opsToRemove) = 0;

  /// Mode-aware rebase for all EDT users
  virtual bool rebaseAllUsersToAcquireViewImpl(DbAcquireOp acquire,
                                               ArtsCodegen &AC) = 0;

  //===--------------------------------------------------------------------===//
  /// Static Utility Methods for Datablock Access Pattern Transformations
  //===--------------------------------------------------------------------===//

  /// Rewrite memref access to use db_ref pattern.
  static bool
  rewriteAccessWithDbPattern(Operation *op, Value dbPtr, Type elementType,
                             unsigned outerCount, ArtsCodegen &AC,
                             llvm::SetVector<Operation *> &opsToRemove);

  /// Rebase operation indices to acquired view's local coordinates.
  /// This static method creates the appropriate DbRewriter instance and
  /// delegates to the virtual rebaseToAcquireViewImpl method.
  static bool rebaseToAcquireView(Operation *op, DbAcquireOp acquire,
                                  Value dbPtr, Type elementType,
                                  ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove);

  /// Rebase all users of acquired blockArg to local coordinates.
  /// This static method creates the appropriate DbRewriter instance and
  /// delegates to the virtual rebaseAllUsersToAcquireViewImpl method.
  static bool rebaseAllUsersToAcquireView(DbAcquireOp acquire, ArtsCodegen &AC);

  //===--------------------------------------------------------------------===//
  /// Factory Method
  //===--------------------------------------------------------------------===//

  /// Create appropriate rewriter based on mode
  /// oldElementSizes: For element-wise mode, the old allocation's element sizes
  ///                  for proper stride computation (supports dynamic strides)
  static std::unique_ptr<DbRewriter>
  create(bool isChunked, Value chunkSize, Value startChunk, Value elemOffset,
         Value elemSize, unsigned newOuterRank, unsigned newInnerRank,
         ValueRange oldElementSizes = {});
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_DBREWRITER_H
