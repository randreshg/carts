///==========================================================================///
/// File: DatablockUtils.h
///
/// Utility functions for working with ARTS datablocks (DbAllocOp, DbAcquireOp).
/// This module consolidates all datablock-related utilities including tracing,
/// size/offset extraction, and stride computation.
///==========================================================================///

#ifndef CARTS_UTILS_DATABLOCKUTILS_H
#define CARTS_UTILS_DATABLOCKUTILS_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// Partition Mode Enum
///===----------------------------------------------------------------------===///
/// Partition mode for datablock access patterns.
/// Determined from DbAcquireOp structure - no attributes needed!
enum class PartitionMode { Coarse, ElementWise, Chunked };

///===----------------------------------------------------------------------===///
/// Datablock Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with ARTS datablocks (DbAllocOp, DbAcquireOp).
class DatablockUtils {
public:
  ///===----------------------------------------------------------------------===///
  /// Datablock Tracing Utilities
  ///===----------------------------------------------------------------------===///
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

  ///===----------------------------------------------------------------------===///
  /// Datablock Size and Offset Extraction
  ///===----------------------------------------------------------------------===///
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

  /// Extract offsets from a datablock pointer value.
  /// Only DbAcquireOp has offsets.
  static SmallVector<Value> getOffsetsFromDb(Value datablockPtr);

  /// Check if a datablock operation has a single size of 1.
  static bool hasSingleSize(Operation *dbOp);

  /// Check if allocation is coarse-grained (all sizes == 1).
  static bool isCoarseGrained(DbAllocOp alloc);

  ///===----------------------------------------------------------------------===///
  /// Partition Mode Detection
  ///===----------------------------------------------------------------------===///
  /// Structure-based mode detection - no attributes needed!

  /// Get partition mode from DbAcquireOp structure.
  static PartitionMode getPartitionMode(DbAcquireOp acquire);

  /// Get partition mode from DbAllocOp
  static PartitionMode getPartitionMode(DbAllocOp alloc);

  static bool isChunked(DbAcquireOp acquire) {
    return getPartitionMode(acquire) == PartitionMode::Chunked;
  }

  static bool isElementWise(DbAcquireOp acquire) {
    return getPartitionMode(acquire) == PartitionMode::ElementWise;
  }

  static bool isCoarse(DbAcquireOp acquire) {
    return getPartitionMode(acquire) == PartitionMode::Coarse;
  }

  ///===----------------------------------------------------------------------===///
  /// Datablock Stride Computation
  ///===----------------------------------------------------------------------===///
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

  ///===----------------------------------------------------------------------===//
  /// Access Mode and Hints Analysis
  ///===----------------------------------------------------------------------===//

  /// Check if a DbAcquireOp has static (constant) offset and size hints.
  /// Returns true if both offset and size hints are either empty or constant.
  static bool hasStaticHints(DbAcquireOp acqOp);

  /// Check if an ArtsMode is a writer mode (out or inout).
  static bool isWriterMode(ArtsMode mode);

  ///===----------------------------------------------------------------------===///
  /// Offset Dependency and Chunk Size Analysis
  ///===----------------------------------------------------------------------===///
  /// Functions for analyzing value dependencies on offsets and extracting
  /// base chunk sizes from size hints.

  /// Check if a value depends on a partition offset (ignoring numeric casts).
  /// Used to determine data dependencies in index expressions for partitioning.
  static bool dependsOnOffset(Value v, Value offset);

  /// Try to extract an offset-independent base chunk size from size hints.
  static Value extractBaseChunkSizeCandidate(Value offsetHint, Value sizeHint,
                                             int depth = 0);

  /// Find the EDT operation that uses a DbControlOp result.
  static Operation *findUserEdt(DbControlOp dbControl);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_DATABLOCKUTILS_H
