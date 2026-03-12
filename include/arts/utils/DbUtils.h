///==========================================================================///
/// File: DbUtils.h
///
/// Utility functions for working with ARTS DBs (DbAllocOp, DbAcquireOp).
/// This module consolidates all DB-related utilities including tracing,
/// size/offset extraction, and stride computation.
///==========================================================================///

#ifndef CARTS_UTILS_DBUTILS_H
#define CARTS_UTILS_DBUTILS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Value.h"
#include "mlir/IR/Visitors.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/SetVector.h"
#include <optional>

namespace mlir {
namespace arts {

/// Information extracted from a datablock operation for lowering.
/// Consolidates sizes, offsets, indices, and single-element detection into
/// a single struct returned by extractDbLoweringInfo().
struct DbLoweringInfo {
  SmallVector<Value> sizes;
  SmallVector<Value> offsets;
  SmallVector<Value> indices;
  bool isSingleElement = false;
};

///===----------------------------------------------------------------------===///
/// Datablock Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with ARTS datablocks (DbAllocOp, DbAcquireOp).
class DbUtils {
public:
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

  /// Trace a dependency back to its root DbAllocOp.
  /// Returns {guid, ptr} from the DbAllocOp, or nullopt if not found.
  static std::optional<std::pair<Value, Value>> traceToDbAlloc(Value dep);

  /// Finds the datablock-related operation (DbAllocOp or DbAcquireOp)
  /// associated with the given value. The depth parameter prevents infinite
  /// recursion in circular acquire chains.
  static Operation *getUnderlyingDb(Value v, unsigned depth = 0);

  /// Finds the DbAllocOp associated with the given value.
  /// Traces through DbAcquireOp chains to find the root allocation.
  static Operation *getUnderlyingDbAlloc(Value v);

  /// Conservative memory-object identity for memref-like values.
  /// Preference order:
  /// 1. Same root DbAllocOp (when both values map to a DB allocation)
  /// 2. Same underlying defining operation (modulo numeric casts)
  /// 3. Same SSA value (modulo numeric casts)
  static bool isSameMemoryObject(Value lhsMemref, Value rhsMemref);

  ///===----------------------------------------------------------------------===////
  /// Datablock Lowering Info Extraction
  ///===----------------------------------------------------------------------===////

  /// Extract lowering info (sizes, offsets, indices, isSingleElement) from
  /// a datablock operation. Handles DbAcquireOp and DepDbAcquireOp by
  /// extracting dependency sizes/offsets/indices; for other ops (e.g.
  /// DbAllocOp) falls back to op.getSizes() with empty offsets/indices.
  template <typename OpType>
  static DbLoweringInfo extractDbLoweringInfo(OpType op);

  ///===----------------------------------------------------------------------===////
  /// Datablock Size and Offset Extraction
  ///===----------------------------------------------------------------------===////
  /// Functions for extracting sizes, element sizes, and offsets from datablocks

  /// Extract sizes from a datablock operation.
  /// Supports DbAllocOp, DbAcquireOp, and DepDbAcquireOp.
  static SmallVector<Value> getSizesFromDb(Operation *dbOp);

  /// Extract element sizes from a datablock operation.
  /// Only DbAllocOp has element sizes.
  static SmallVector<Value> getElementSizesFromDb(Operation *dbOp);

  /// Extract sizes from a datablock pointer value.
  /// Traces back to the original DbAllocOp or DbAcquireOp that created it.
  static SmallVector<Value> getSizesFromDb(Value datablockPtr);

  /// Extract the dependency iteration sizes used for EDT dependency counting
  /// and record_dep lowering.
  /// For block/stencil acquires that carry partition slices, this returns the
  /// partition_sizes entry so runtime dependencies are scoped to the task
  /// slice.
  static SmallVector<Value> getDependencySizesFromDb(Operation *dbOp);

  /// Extract dependency iteration sizes from a datablock value.
  static SmallVector<Value> getDependencySizesFromDb(Value datablockPtr);

  /// Extract the dependency iteration offsets used for EDT dependency lowering.
  /// For block/stencil acquires with partition slices, this returns the
  /// partition_offsets entry.
  static SmallVector<Value> getDependencyOffsetsFromDb(Operation *dbOp);

  /// Extract dependency iteration offsets from a datablock value.
  static SmallVector<Value> getDependencyOffsetsFromDb(Value datablockPtr);

  /// Extract offsets from a datablock pointer value.
  /// Only DbAcquireOp has offsets.
  static SmallVector<Value> getOffsetsFromDb(Value datablockPtr);

  /// Check if a datablock operation has a single size of 1.
  static bool hasSingleSize(Operation *dbOp);

  /// Check if allocation is coarse-grained (all sizes == 1).
  static bool isCoarseGrained(DbAllocOp alloc);

  /// Check if allocation is fine-grained (partitioned into multiple DBs).
  static bool isFineGrained(DbAllocOp alloc);

  ///===----------------------------------------------------------------------===////
  /// Partition Mode Detection
  ///===----------------------------------------------------------------------===////
  /// Structure-based mode detection - no attributes needed!

  /// Get partition mode from DbAcquireOp structure.
  static PartitionMode getPartitionModeFromStructure(DbAcquireOp acquire);

  /// Get partition mode from DbAllocOp
  static PartitionMode getPartitionModeFromStructure(DbAllocOp alloc);

  static bool isBlock(DbAcquireOp acquire) {
    return getPartitionModeFromStructure(acquire) == PartitionMode::block;
  }

  static bool isElementWise(DbAcquireOp acquire) {
    return getPartitionModeFromStructure(acquire) ==
           PartitionMode::fine_grained;
  }

  static bool isCoarse(DbAcquireOp acquire) {
    return getPartitionModeFromStructure(acquire) == PartitionMode::coarse;
  }

  ///===----------------------------------------------------------------------===////
  /// Datablock Stride Computation
  ///===----------------------------------------------------------------------===////
  /// Functions for computing strides for row-major indexing.
  /// For sizes = [D0, D1, D2, ...], stride = D1 * D2 * ... (TRAILING
  /// dimensions). This follows row-major linearization: index = i0 * stride +
  /// ...
  ///
  /// Examples:
  ///   [4, 16]     -> stride = 16
  ///   [8, 4, 2]   -> stride = 4 * 2 = 8
  ///   [64]        -> stride = 1 (single dimension)

  /// Compute static stride from sizes (compile-time constant).
  /// Returns std::nullopt if any trailing dimension is dynamic.
  static std::optional<int64_t> getStaticStride(ValueRange sizes);

  /// Overload for MemRefType shape.
  static std::optional<int64_t> getStaticStride(MemRefType memrefType);

  /// Get element stride from DbAllocOp (uses getElementSizes()).
  /// This is the stride for linearized access WITHIN each datablock element.
  static std::optional<int64_t> getStaticElementStride(DbAllocOp alloc);

  /// Get outer stride from DbAllocOp (uses getSizes()).
  /// This is the stride for linearized access ACROSS datablocks.
  static std::optional<int64_t> getStaticOuterStride(DbAllocOp alloc);

  /// Compute stride as a Value (handles both static and dynamic dimensions).
  /// If all trailing dimensions are static, returns an arith::ConstantIndexOp.
  /// If any trailing dimension is dynamic, generates arith::MulIOp chain.
  /// For single-dimension [N], returns constant 1.
  /// Returns nullptr if sizes is empty.
  static Value getStrideValue(OpBuilder &builder, Location loc,
                              ValueRange sizes);

  /// Get element stride Value from DbAllocOp (uses getElementSizes()).
  static Value getElementStrideValue(OpBuilder &builder, Location loc,
                                     DbAllocOp alloc);

  /// Get outer stride Value from DbAllocOp (uses getSizes()).
  static Value getOuterStrideValue(OpBuilder &builder, Location loc,
                                   DbAllocOp alloc);

  ///===----------------------------------------------------------------------===///
  /// Access Mode and Hints Analysis
  ///===----------------------------------------------------------------------===///

  /// Check if a DbAcquireOp has static (constant) offset and size hints.
  /// Returns true if both offset and size hints are either empty or constant.
  static bool hasStaticHints(DbAcquireOp acqOp);

  /// Check if an ArtsMode is a writer mode (out or inout).
  static bool isWriterMode(ArtsMode mode);

  ///===----------------------------------------------------------------------===///
  /// Offset Dependency and Block Size Analysis
  ///===----------------------------------------------------------------------===///
  /// Functions for analyzing value dependencies on offsets and extracting
  /// base block sizes from size hints.

  /// Check if a value depends on a partition offset (ignoring numeric casts).
  /// Used to determine data dependencies in index expressions for partitioning.
  static bool dependsOnOffset(Value v, Value offset);

  /// Try to extract an offset-independent base block size from size hints.
  static Value extractBaseBlockSizeCandidate(Value offsetHint, Value sizeHint,
                                             int depth = 0);

  /// Find the EDT operation that uses a DbControlOp result.
  static Operation *findUserEdt(DbControlOp dbControl);

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

  /// Extract index operands from a memory access op.
  /// Supports memref/affine load/store operations.
  static SmallVector<Value> getMemoryAccessIndices(Operation *memOp);

  /// Return decoded memory-access information for a load/store op.
  /// Supports memref/affine load/store operations.
  static std::optional<MemoryAccessInfo> getMemoryAccessInfo(Operation *memOp);

  /// Return the memref/base object accessed by a load/store op.
  static Value getAccessedMemref(Operation *memOp);

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

  ///===----------------------------------------------------------------------===////
  /// Multi-Entry Stencil Pattern Detection
  ///===----------------------------------------------------------------------===////

  /// Try to extract a constant offset between two index values.
  /// Returns the offset if idx = base + constant (or base - constant).
  /// For stencil patterns like [i-1, i, i+1], this finds the offset from base.
  static std::optional<int64_t> getConstantOffsetBetween(Value idx, Value base);

  /// Check if a multi-entry acquire has a stencil access pattern.
  /// Stencil patterns have indices that differ by small constant offsets,
  /// e.g., [i-1, i, i+1] for a 1D stencil or [i-1, i, i+1] x [j] for 2D.
  ///
  /// Returns true if partition indices across entries form a stencil pattern.
  /// Outputs halo bounds in minOffset (negative for left halo) and maxOffset.
  static bool hasMultiEntryStencilPattern(DbAcquireOp acquire,
                                          int64_t &minOffset,
                                          int64_t &maxOffset);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_DBUTILS_H
