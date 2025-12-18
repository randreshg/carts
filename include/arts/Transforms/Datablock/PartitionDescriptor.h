///==========================================================================///
/// File: PartitionDescriptor.h
///
/// Complete description of how a datablock is partitioned.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_PARTITIONDESCRIPTOR_H
#define ARTS_TRANSFORMS_DATABLOCK_PARTITIONDESCRIPTOR_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// PartitionDescriptor - Complete description of how a datablock is partitioned
///
/// Pure data structure providing a SINGLE SOURCE OF TRUTH for all index
/// localization operations. Created and consumed within DbAllocPromotion
/// transformation scope.
///
/// Key Concepts:
/// - GlobalIndex: Index in the original (unpromoted) allocation
/// - LocalIndex: Index within this partition's acquired view (starts at 0)
/// - ChunkIndex: Which chunk within the partition (for chunked mode)
/// - IntraChunkIndex: Position within a single chunk (for chunked mode)
///
/// This class fixes the bug where linearized memref offsets were not scaled
/// by stride, causing incorrect memory access patterns.
///===----------------------------------------------------------------------===///
class PartitionDescriptor {
public:
  /// Partition strategy - HOW was the allocation partitioned?
  enum class Strategy {
    None,       /// Not partitioned - single chunk containing everything
    Chunked,    /// Fixed-size chunks (div/mod transformation)
    ElementWise /// Each element/row is a separate chunk
  };

  Strategy strategy = Strategy::None;

  /// The dimension along which partitioning occurs (0 = outermost)
  /// ONLY this dimension needs index transformation; others pass through
  unsigned partitionDim = 0;

  /// First element this partition owns (global element index)
  Value globalStartElement;

  /// Number of elements this partition owns
  Value elementCount;

  /// Physical chunk size (elements per chunk) - for chunked mode
  Value chunkSize;

  /// First chunk this partition acquires (in global chunk space) - for chunked
  Value globalStartChunk;

  //===--------------------------------------------------------------------===//
  /// Factory Methods
  //===--------------------------------------------------------------------===//

  /// Create descriptor for chunked partition
  static PartitionDescriptor forChunked(Value chunkSize, Value startChunk,
                                        Value startElement, Value elementCount,
                                        unsigned partitionDim = 0);

  /// Create descriptor for element-wise partition
  static PartitionDescriptor forElementWise(Value startElement,
                                            Value elementCount,
                                            unsigned partitionDim = 0);

  //===--------------------------------------------------------------------===//
  /// Index Transformation API - The Core Interface
  //===--------------------------------------------------------------------===//

  /// Result of localizing indices
  struct LocalizedIndices {
    SmallVector<Value> dbRefIndices;  /// Indices for arts.db_ref operation
    SmallVector<Value> memrefIndices; /// Indices for memref load/store
  };

  /// Transform global multi-dimensional indices to local
  ///
  /// INPUT: globalIndices in original allocation's coordinate space
  /// OUTPUT: {dbRefIndices, memrefIndices} for the promoted structure
  ///
  /// For partitionDim:
  ///   Chunked: dbRef = (global / chunkSize) - globalStartChunk
  ///            memref = global % chunkSize
  ///   ElementWise: dbRef = global - globalStartElement
  ///                memref = (remaining dims)
  /// Other dims: pass through unchanged
  LocalizedIndices localize(ArrayRef<Value> globalIndices,
                            unsigned newOuterRank, unsigned newInnerRank,
                            OpBuilder &builder, Location loc) const;

  /// Transform a linearized global index (for flattened 1D memrefs)
  ///
  /// For flattened memrefs where index = row*stride + col
  /// scales offset by stride before subtracting!
  ///
  /// CRITICAL FIX for bug:
  ///   WRONG:   localLinear = globalLinear - offset
  ///   CORRECT: localLinear = globalLinear - (offset * stride)
  LocalizedIndices localizeLinearized(Value globalLinearIndex, Value stride,
                                      unsigned newOuterRank,
                                      unsigned newInnerRank, OpBuilder &builder,
                                      Location loc) const;

  //===--------------------------------------------------------------------===//
  /// Query Helpers
  //===--------------------------------------------------------------------===//

  bool isChunked() const { return strategy == Strategy::Chunked; }
  bool isElementWise() const { return strategy == Strategy::ElementWise; }
  bool isPartitioned() const { return strategy != Strategy::None; }
};

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_DATABLOCK_PARTITIONDESCRIPTOR_H
