///==========================================================================///
/// File: PartitionDescriptor.cpp
///
/// Implementation of PartitionDescriptor for index localization.
///==========================================================================///

#include "arts/Transforms/Datablock/PartitionDescriptor.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

using namespace mlir;
using namespace mlir::arts;

PartitionDescriptor PartitionDescriptor::forChunked(Value chunkSize,
                                                    Value startChunk,
                                                    Value startElement,
                                                    Value elementCount,
                                                    unsigned partitionDim) {
  PartitionDescriptor desc;
  desc.strategy = Strategy::Chunked;
  desc.partitionDim = partitionDim;
  desc.globalStartElement = startElement;
  desc.elementCount = elementCount;
  desc.chunkSize = chunkSize;
  desc.globalStartChunk = startChunk;
  return desc;
}

PartitionDescriptor PartitionDescriptor::forElementWise(Value startElement,
                                                        Value elementCount,
                                                        unsigned partitionDim) {
  PartitionDescriptor desc;
  desc.strategy = Strategy::ElementWise;
  desc.partitionDim = partitionDim;
  desc.globalStartElement = startElement;
  desc.elementCount = elementCount;
  return desc;
}

PartitionDescriptor::LocalizedIndices
PartitionDescriptor::localize(ArrayRef<Value> globalIndices,
                              unsigned newOuterRank, unsigned newInnerRank,
                              OpBuilder &builder, Location loc) const {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  if (!isPartitioned()) {
    // No partition - pass through with zero db_ref index
    result.dbRefIndices.push_back(zero());
    for (Value idx : globalIndices)
      result.memrefIndices.push_back(idx);
    return result;
  }

  // Process each dimension
  for (unsigned dim = 0; dim < globalIndices.size(); ++dim) {
    Value globalIdx = globalIndices[dim];

    if (dim == partitionDim) {
      // THIS is the partitioned dimension - apply transformation
      if (isChunked()) {
        // CHUNKED MODE:
        // dbRefIdx = (globalIdx / chunkSize) - globalStartChunk
        // memrefIdx = globalIdx % chunkSize
        Value physicalChunk =
            builder.create<arith::DivUIOp>(loc, globalIdx, chunkSize);
        Value relativeChunk =
            builder.create<arith::SubIOp>(loc, physicalChunk, globalStartChunk);
        Value intraChunkIdx =
            builder.create<arith::RemUIOp>(loc, globalIdx, chunkSize);

        result.dbRefIndices.push_back(relativeChunk);
        result.memrefIndices.push_back(intraChunkIdx);
      } else {
        // ELEMENT-WISE MODE:
        // dbRefIdx = globalIdx - globalStartElement
        // memrefIdx = 0 (or remaining inner dims handled below)
        Value localIdx =
            builder.create<arith::SubIOp>(loc, globalIdx, globalStartElement);
        result.dbRefIndices.push_back(localIdx);
        // Don't add to memrefIndices here - inner dims are added below
      }
    } else {
      // Non-partitioned dimension - pass through to memref indices
      result.memrefIndices.push_back(globalIdx);
    }
  }

  // Ensure we have at least one index for each
  if (result.dbRefIndices.empty())
    result.dbRefIndices.push_back(zero());
  if (result.memrefIndices.empty())
    result.memrefIndices.push_back(zero());

  return result;
}

PartitionDescriptor::LocalizedIndices PartitionDescriptor::localizeLinearized(
    Value globalLinearIndex, Value stride, unsigned newOuterRank,
    unsigned newInnerRank, OpBuilder &builder, Location loc) const {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  if (!isPartitioned()) {
    // No partition - pass through
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(globalLinearIndex);
    return result;
  }

  if (isChunked()) {
    // CHUNKED LINEARIZED:
    // De-linearize to get the row index (partitioned dimension)
    // globalLinear = globalRow * stride + col
    // globalRow = globalLinear / stride
    // col = globalLinear % stride

    Value globalRow =
        builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
    Value col = builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

    // dbRefIdx = (globalRow / chunkSize) - globalStartChunk
    // localRow = globalRow % chunkSize
    Value physicalChunk =
        builder.create<arith::DivUIOp>(loc, globalRow, chunkSize);
    Value relativeChunk =
        builder.create<arith::SubIOp>(loc, physicalChunk, globalStartChunk);
    Value intraChunkRow =
        builder.create<arith::RemUIOp>(loc, globalRow, chunkSize);

    result.dbRefIndices.push_back(relativeChunk);

    // Re-linearize local position: localLinear = localRow * stride + col
    Value localLinear =
        builder.create<arith::MulIOp>(loc, intraChunkRow, stride);
    localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
    result.memrefIndices.push_back(localLinear);

  } else {
    // ELEMENT-WISE LINEARIZED:
    // The KEY FIX: scale the element offset by stride before subtracting!
    //
    // globalLinear = (globalStartElement + localRow) * stride + col
    // localLinear = localRow * stride + col
    // Therefore: localLinear = globalLinear - (globalStartElement * stride)
    //
    // NOT: localLinear = globalLinear - globalStartElement  <-- THIS WAS WRONG!

    Value scaledOffset =
        builder.create<arith::MulIOp>(loc, globalStartElement, stride);
    Value localLinear =
        builder.create<arith::SubIOp>(loc, globalLinearIndex, scaledOffset);

    // For element-wise, the db_ref index is the row index relative to start
    // dbRefIdx = globalRow - globalStartElement
    Value globalRow =
        builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
    Value dbRefIdx =
        builder.create<arith::SubIOp>(loc, globalRow, globalStartElement);

    result.dbRefIndices.push_back(dbRefIdx);
    result.memrefIndices.push_back(localLinear);
  }

  return result;
}
