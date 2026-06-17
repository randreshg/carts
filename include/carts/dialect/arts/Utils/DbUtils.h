///==========================================================================///
/// File: DbUtils.h
///
/// Utility functions for working with ARTS DBs (DbAllocOp, DbAcquireOp).
/// This module consolidates all DB-related utilities including tracing,
/// size/offset extraction, and stride computation.
///==========================================================================///

#ifndef CARTS_UTILS_DBUTILS_H
#define CARTS_UTILS_DBUTILS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/Visitors.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/SetVector.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace carts::arts {

/// Computes the byte size of a DB element type. Memref element types require
/// all static dimensions; dynamic shapes return 0.
uint64_t getElementTypeByteSize(Type elementType);

/// Build the uniform memref type used for an ARTS DB element payload.
MemRefType getElementMemRefType(Type elementType, unsigned rank);
MemRefType getElementMemRefType(Type elementType, ArrayRef<Value> elementSizes);

/// Combine two ARTS access modes and return the least restrictive mode.
ArtsMode combineAccessModes(ArtsMode mode1, ArtsMode mode2);

///===----------------------------------------------------------------------===///
/// Datablock Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with ARTS datablocks (DbAllocOp, DbAcquireOp).
class DbUtils {
public:
  static constexpr int64_t kSmallCoarseReadOnlyElementLimit = 16 * 1024;

  enum class MemoryAccessKind { Read, Write };

  struct MemoryAccessInfo {
    Operation *op = nullptr;
    Value memref;
    SmallVector<Value> indices;
    MemoryAccessKind kind = MemoryAccessKind::Read;

    bool isRead() const { return kind == MemoryAccessKind::Read; }
    bool isWrite() const { return kind == MemoryAccessKind::Write; }
  };

  ///===----------------------------------------------------------------------===////
  /// Datablock Tracing Utilities
  ///===----------------------------------------------------------------------===////
  /// Functions for tracing values back to their source datablock operations

  /// Finds the datablock-related operation (DbAllocOp or DbAcquireOp)
  /// associated with the given value. The depth parameter prevents infinite
  /// recursion in circular acquire chains.
  static Operation *getUnderlyingDb(Value v, unsigned depth = 0);

  /// Finds the DbAllocOp associated with the given value.
  /// Traces through DbAcquireOp chains to find the root allocation.
  static Operation *getUnderlyingDbAlloc(Value v);

  /// Typed wrapper for getUnderlyingDbAlloc.
  static DbAllocOp getUnderlyingDbAllocOp(Value v);

  /// Trace a GUID value through acquire chains to find the originating
  /// DbAllocOp. Returns nullptr if the GUID does not trace to an allocation.
  static DbAllocOp getAllocOpFromGuid(Value dbGuid);

  ///===----------------------------------------------------------------------===////
  /// Datablock Size and Offset Extraction
  ///===----------------------------------------------------------------------===////
  /// Functions for extracting sizes, element sizes, and offsets from datablocks

  /// Extract sizes from a datablock operation.
  /// Supports DbAllocOp and DbAcquireOp.
  static SmallVector<Value> getSizesFromDb(Operation *dbOp);

  /// Extract sizes from a datablock pointer value.
  /// Traces back to the original DbAllocOp or DbAcquireOp that created it.
  static SmallVector<Value> getSizesFromDb(Value dbPtr);

  /// Extract the dependency iteration sizes used for EDT dependency counting
  /// and record_dep lowering.
  /// For block/stencil acquires, this prefers a cached DB-space
  /// window when available; otherwise it falls back to the acquire's explicit
  /// DB-space sizes.
  static SmallVector<Value> getDepSizesFromDb(Operation *dbOp);

  /// Extract dependency iteration sizes from a datablock value.
  static SmallVector<Value> getDepSizesFromDb(Value dbPtr);

  /// Extract the dependency iteration offsets used for EDT dependency lowering.
  /// For block/stencil acquires, this prefers a cached DB-space
  /// window when available; otherwise it falls back to the acquire's explicit
  /// DB-space offsets.
  static SmallVector<Value> getDepOffsetsFromDb(Operation *dbOp);

  /// Extract dependency iteration offsets from a datablock value.
  static SmallVector<Value> getDepOffsetsFromDb(Value dbPtr);

  ///===----------------------------------------------------------------------===////
  /// DB-space Window Facts
  ///===----------------------------------------------------------------------===////
  /// A partial acquire reads a strict sub-window of its DB block. ARTS commits
  /// halo_slice only as stencil reach metadata; ARTS-RT requires explicit
  /// element offsets/sizes for byte-window lowering. These predicates name the
  /// set that requires such a window and whether it is present.

  /// Return true when this acquire realizes a halo sub-window: a stencil /
  /// block-halo acquire that carries a concrete stencil access extent (matching
  /// lower/upper offsets).
  /// Without that extent the acquire reads the whole block and needs no window.
  static bool acquiresPartialHaloWindow(DbAcquireOp acquire);

  /// Return true when this acquire already carries explicit element-space
  /// offsets/sizes operands that ARTS-RT can lower mechanically.
  static bool hasCommittedDbSpaceWindow(DbAcquireOp acquire);

  ///===----------------------------------------------------------------------===///
  /// Access Mode and Hints Analysis
  ///===----------------------------------------------------------------------===///

  /// Check if an ArtsMode is a writer mode (out or inout).
  static bool isWriterMode(ArtsMode mode);

  /// Return true for DB pointer wrappers whose element payload is i1. These
  /// are compiler control-token DBs, not user data.
  static bool isI1DbPtrType(Type type);

  /// Return the static logical element count for a DB allocation when all
  /// outer DB counts and payload element sizes fold to non-negative constants.
  static std::optional<int64_t> getStaticElementCount(DbAllocOp alloc);

  /// Return true for coarse, non-control user data DBs.
  static bool isCoarseUserDataDb(DbAllocOp alloc);

  /// Return true for small coarse user data DBs that are cheap enough to
  /// transport read-only to distributed tasks instead of forcing the whole
  /// task back to intranode execution.
  static bool isSmallCoarseUserDataDb(
      DbAllocOp alloc, int64_t maxElements = kSmallCoarseReadOnlyElementLimit);

  /// Return true when current DB facts cannot support distributed ownership and
  /// therefore cannot anchor an internode launch.
  static bool cannotUseDistributedOwnership(DbAllocOp alloc);

  /// Return true when a dependency is a small read-only coarse DB that is cheap
  /// enough to transport to an internode task without forcing local placement.
  static bool isAllowedSmallReadOnlyCoarseDep(Value dep, DbAllocOp alloc);

  /// Return true when a read-only coarse DB dependency has an explicit
  /// transport facts that allow internode tasks to consume it.
  static bool isAllowedReadOnlyCoarseDep(Value dep, DbAllocOp alloc);

  /// Return true for the explicit ARTS storage bridge allocation that carries
  /// host-whole data into compute-block DB shape.
  static bool isHostWholeToComputeBlockBridgeDb(DbAllocOp alloc);

  /// Return true for generated bridge EDTs that move data between a local
  /// coarse host allocation and a distributed compute-block bridge allocation.
  static bool isHostWholeToComputeBlockBridgeMovement(EdtOp edt);

  /// Return true when this dependency forces an otherwise distributed task to
  /// stay local because its root DB allocation was rejected for distributed
  /// ownership.
  static bool requiresLocalLaunchForDistributedDep(Value dep);

  /// Return true when any dependency on the EDT prevents internode placement.
  static bool hasLocalOnlyDistributedLaunchDependency(EdtOp edt);

  /// Convert ArtsMode (in/out/inout) to DbMode (read/write).
  /// ArtsMode::in maps to DbMode::read; out and inout map to DbMode::write.
  static DbMode convertArtsModeToDbMode(ArtsMode mode);

  /// Ordered runtime DB mode for an access mode: ArtsMode::in maps to ro; out
  /// and inout map to ew (the ordered exclusive write). This is the committed
  /// verdict when no unordered (rw) policy applies.
  static RuntimeDbMode orderedRuntimeDbMode(ArtsMode mode);

  ///===----------------------------------------------------------------------===///
  /// Offset Dep and Block Size Analysis
  ///===----------------------------------------------------------------------===///
  /// Functions for analyzing value dependencies on offsets and extracting
  /// base block sizes from size hints.

  /// Pick a representative partition offset (prefer non-constant).
  /// Returns the chosen offset and its index via outIdx when provided.
  static Value pickRepresentativePartitionOffset(ArrayRef<Value> offsets,
                                                 unsigned *outIdx = nullptr);

  /// Pick the partition size corresponding to the chosen offset index.
  static Value pickRepresentativePartitionSize(ArrayRef<Value> sizes,
                                               unsigned idx);

  ///===----------------------------------------------------------------------===///
  /// Index Chain Utilities
  ///===----------------------------------------------------------------------===///

  /// Collect full index chain from DbRefOp indices plus memory operation
  /// indices. For accesses through view-like ops (e.g., subview), dynamic
  /// forwarding offsets are inserted before terminal memory-op indices.
  /// Returns a combined chain beginning with DbRef indices.
  static SmallVector<Value> collectFullIndexChain(DbRefOp dbRef,
                                                  Operation *memOp);

  /// Return the accessed memref value from a load/store op, or a null Value.
  /// Supports memref and affine load/store operations.
  static Value getAccessedMemref(Operation *memOp);

  /// Extract index operands from a memory access op.
  /// Supports memref/affine load/store operations.
  static SmallVector<Value> getMemoryAccessIndices(Operation *memOp);

  /// Return decoded memory-access information for a load/store op.
  /// Supports memref/affine load/store operations.
  static std::optional<MemoryAccessInfo> getMemoryAccessInfo(Operation *memOp);

  /// Return true when two scopes touch the same underlying DB allocation or
  /// raw memref and at least one side writes that root.
  static bool hasSharedWritableRootConflict(Operation *lhs, Operation *rhs);

  /// Collect load/store operations reachable from source through view-like
  /// forwarding ops (casts, subviews, etc.). Optionally restrict to a scope.
  static void collectReachableMemoryOps(Value source,
                                        llvm::SetVector<Operation *> &memOps,
                                        Region *scope = nullptr);

  /// Visit load/store operations reachable from source through view-like
  /// forwarding ops (casts, subviews, etc.). The callback may interrupt early.
  static void forEachReachableMemoryAccess(
      Value source,
      llvm::function_ref<WalkResult(const MemoryAccessInfo &)> visitor,
      Region *scope = nullptr);

  static ArtsMode inferEdtAccessMode(Operation *underlyingOp, EdtOp edt);
  static ArtsMode classifyMemrefUserAccessMode(Operation *op,
                                               Operation *underlyingOp);
  static bool opMatchesAccessMode(Operation *op, Operation *underlyingOp,
                                  ArtsMode requestedMode);
  static bool accessModeCanSeedNestedAcquire(ArtsMode availableMode,
                                             ArtsMode requestedMode);
  static bool isCoarseGrained(DbAllocOp alloc);
  static bool isSameMemoryObject(Value lhsMemref, Value rhsMemref);

  /// Return true when every reachable use of source is forwarding or cleanup
  /// plumbing. Collected operations can be erased to remove the dead DB chain.
  static bool collectCleanupOnlyUseChain(Value source,
                                         llvm::SetVector<Operation *> &ops,
                                         Region *scope = nullptr);

  /// Return true when an i1 control-token DB is only consumed by cleanup
  /// plumbing and stores of literal true. Collected operations can be erased
  /// after removing the owning EDT dependency slot.
  static bool collectTrueOnlyControlTokenUseChain(
      Value source, llvm::SetVector<Operation *> &ops, Region *scope = nullptr);

  ///===----------------------------------------------------------------------===///
  /// Element-to-Block Space Slice Conversion
  ///===----------------------------------------------------------------------===///

  /// Convert element-space offsets and sizes to block-space offsets and sizes.
  /// For each dimension, computes:
  ///   startBlock = elementOffset / blockSpan
  ///   endBlock   = (elementOffset + elementSize - 1) / blockSpan
  ///   blockCount = endBlock - startBlock + 1  (clamped to [0, totalBlocks])
  static void convertElementSliceToBlockSlice(
      OpBuilder &builder, Location loc, ValueRange elementOffsets,
      ValueRange elementSizes, ValueRange blockSpans,
      ValueRange totalBlockCounts, SmallVectorImpl<Value> &blockOffsets,
      SmallVectorImpl<Value> &blockSizes);

  /// Overlay a normalized owner-space prefix onto an existing DB-space slice
  /// without collapsing the source DB rank. This keeps untouched owner slots at
  /// their current range (or full-range) while replacing the leading
  /// normalized slots produced by convertElementSliceToBlockSlice().
  static void mergeNormalizedBlockSlice(
      OpBuilder &builder, Location loc, ValueRange existingOffsets,
      ValueRange existingSizes, ValueRange totalBlockCounts,
      ValueRange normalizedOffsets, ValueRange normalizedSizes,
      SmallVectorImpl<Value> &blockOffsets, SmallVectorImpl<Value> &blockSizes);

  ///===----------------------------------------------------------------------===///
  /// Block Size and Malloc Pattern Extraction
  ///===----------------------------------------------------------------------===///

  /// Extract block size from an explicit partition-size hint.
  /// Handles direct constants, minui/minsi patterns, addi halo patterns,
  /// and maxui clamp patterns with recursive descent up to depth 4.
  static std::optional<int64_t> extractBlockSizeFromHint(Value sizeHint,
                                                         int depth = 0);
};

/// Build a whole-DB acquire window: zero offsets and full alloc sizes.
void buildWholeDbAcquireWindow(OpBuilder &builder, Location loc,
                               DbAllocOp alloc, SmallVectorImpl<Value> &offsets,
                               SmallVectorImpl<Value> &sizes);

/// Create a block-mode acquire for one DB block at the given fold-constant
/// block coordinates. Each coordinate gets a unit extent in DB space.
DbAcquireOp createUnitBlockDbAcquireAtCoords(OpBuilder &builder, Location loc,
                                             ArtsMode mode, DbAllocOp alloc,
                                             ArrayRef<int64_t> blockCoords);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_UTILS_DBUTILS_H
