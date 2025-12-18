///==========================================================================///
/// File: DbTransforms.cpp
/// Unified transformations for datablock operations.
///==========================================================================///

#include "arts/Transforms/DbTransforms.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/OpRemovalManager.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Debug.h"
#include <cstdlib>

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

///===----------------------------------------------------------------------===///
/// ViewCoordinateMap - Global to Local Coordinate Transformation
///
/// DbAcquireOp specifies a view into a datablock with:
///   - indices: Pin specific elements in leading dimensions (reduces
///   dimensionality)
///   - offsets: Starting position for the acquired slice
///   - sizes: Extent of the acquired slice
///
/// Example: 2D array with indices=[%row], offsets=[%col_start], sizes=[%n]
///   Global: arr[%row, %col]
///   Local:  view[0, %col - %col_start]
///         dimension 0 is pinned (indexed), dimension 1 is sliced
///
/// Transformation rules:
///   - Indexed dimensions [0, numIndexedDims): local = 0
///   - Sliced dimensions [numIndexedDims, end):
///     * If globalIdx == offset: local = 0
///     * If globalIdx = offset + delta (provable): local = delta
///     * Otherwise: local = globalIdx - offset (explicit subtraction)
///
/// We always compute local = global - offset for sliced dimensions.
///===----------------------------------------------------------------------===///
struct ViewCoordinateMap {
  /// Number of leading dimensions pinned by DbAcquireOp::indices.
  /// For these dimensions, the local coordinate is always 0.
  unsigned numIndexedDims = 0;

  /// Offsets for sliced dimensions (from DbAcquireOp::offsets).
  /// Applied to dimensions starting at index numIndexedDims.
  /// local[d] = global[d] - sliceOffsets[d - numIndexedDims]
  SmallVector<Value> sliceOffsets;

  /// Create coordinate map from a DbAcquireOp.
  static ViewCoordinateMap fromAcquire(DbAcquireOp acquire);
};

/// Conservative dependency check: does `value` depend on `base` through a small
/// arithmetic chain (add/sub/mul/div/mod/index_cast/ext/trunc)?


///===----------------------------------------------------------------------===///
/// IndexRedistributor - Consolidates index mapping logic for allocation
/// promotion
///
/// Handles redistribution of indices between outer (db_ref) and inner
/// (load/store) dimensions based on allocation mode:
///
/// CHUNKED mode: div/mod on first index
///   newOuter = [index / chunkSize]
///   newInner = [index % chunkSize, remaining...]
///
/// ELEMENT-WISE mode: redistribute indices
///   newOuter = [loadStoreIndices[0..outerRank]]
///   newInner = [loadStoreIndices[outerRank..]]
///===----------------------------------------------------------------------===///
struct IndexRedistributor {
  unsigned outerRank;
  unsigned innerRank;
  bool isChunked;
  Value chunkSize; // Only valid if isChunked

  /// Redistribute indices between outer and inner (for db_refs outside EDTs).
  std::pair<SmallVector<Value>, SmallVector<Value>>
  redistribute(ValueRange loadStoreIndices, OpBuilder &builder,
               Location loc) const {
    SmallVector<Value> newOuter, newInner;
    auto zero = [&]() {
      return builder.create<arith::ConstantIndexOp>(loc, 0);
    };

    if (isChunked && chunkSize) {
      // CHUNKED: div/mod on first load/store index
      if (!loadStoreIndices.empty()) {
        Value idx = loadStoreIndices[0];
        newOuter.push_back(builder.create<arith::DivUIOp>(loc, idx, chunkSize));
        newInner.push_back(builder.create<arith::RemUIOp>(loc, idx, chunkSize));
        for (unsigned i = 1;
             i < loadStoreIndices.size() && newInner.size() < innerRank; ++i)
          newInner.push_back(loadStoreIndices[i]);
      }
    } else {
      // ELEMENT-WISE: redistribute indices from load/store to outer
      for (unsigned i = 0; i < outerRank; ++i) {
        if (i < loadStoreIndices.size())
          newOuter.push_back(loadStoreIndices[i]);
        else
          newOuter.push_back(zero());
      }
      for (unsigned i = 0; i < innerRank; ++i) {
        unsigned srcIdx = outerRank + i;
        if (srcIdx < loadStoreIndices.size())
          newInner.push_back(loadStoreIndices[srcIdx]);
        else
          newInner.push_back(zero());
      }
    }

    // Ensure non-empty
    if (newOuter.empty())
      newOuter.push_back(zero());
    if (newInner.empty())
      newInner.push_back(zero());

    return {newOuter, newInner};
  }

  /// Redistribute indices with offset localization (for acquires inside EDTs).
  /// Element-wise only - localizes global indices to acquired slice.
  std::pair<SmallVector<Value>, SmallVector<Value>>
  redistributeWithOffset(ValueRange loadStoreIndices, Value elemOffset,
                         OpBuilder &builder, Location loc) const {
    SmallVector<Value> newOuter, newInner;
    auto zero = [&]() {
      return builder.create<arith::ConstantIndexOp>(loc, 0);
    };

    for (unsigned i = 0; i < outerRank; ++i) {
      if (i < loadStoreIndices.size()) {
        // Localize the global index to the acquired slice
        Value globalIdx = loadStoreIndices[i];
        Value localIdx =
            builder.create<arith::SubIOp>(loc, globalIdx, elemOffset);
        newOuter.push_back(localIdx);
      } else {
        newOuter.push_back(zero());
      }
    }
    for (unsigned i = 0; i < innerRank; ++i) {
      unsigned srcIdx = outerRank + i;
      if (srcIdx < loadStoreIndices.size())
        newInner.push_back(loadStoreIndices[srcIdx]);
      else
        newInner.push_back(zero());
    }

    // Ensure non-empty
    if (newOuter.empty())
      newOuter.push_back(zero());
    if (newInner.empty())
      newInner.push_back(zero());

    return {newOuter, newInner};
  }
};

} // namespace

///===----------------------------------------------------------------------===///
/// PartitionDescriptor Implementation
///===----------------------------------------------------------------------===///

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

PartitionDescriptor::LocalizedIndices
PartitionDescriptor::localizeLinearized(Value globalLinearIndex, Value stride,
                                        unsigned newOuterRank,
                                        unsigned newInnerRank,
                                        OpBuilder &builder, Location loc) const {
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

///===----------------------------------------------------------------------===///
/// DbRewriter Implementation - Strategy Pattern for Index Localization
///===----------------------------------------------------------------------===///

//===----------------------------------------------------------------------===//
// ChunkedRewriter Implementation
//===----------------------------------------------------------------------===//

ChunkedRewriter::ChunkedRewriter(Value chunkSize, Value startChunk,
                                 Value elemOffset, unsigned outerRank,
                                 unsigned innerRank)
    : chunkSize_(chunkSize), startChunk_(startChunk), elemOffset_(elemOffset),
      outerRank_(outerRank), innerRank_(innerRank) {}

DbRewriter::LocalizedIndices
ChunkedRewriter::localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                          Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("ChunkedRewriter::localize with " << globalIndices.size() << " indices");
  LLVM_DEBUG(llvm::dbgs() << "ChunkedRewriter::localize with "
                          << globalIndices.size() << " indices\n");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  // CHUNKED: dim 0 gets div/mod, rest pass through
  Value globalRow = globalIndices[0];

  // dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, chunkSize_);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  // memrefIdx[0] = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, chunkSize_);
  result.memrefIndices.push_back(localRow);

  // Remaining dimensions pass through unchanged
  for (unsigned i = 1; i < globalIndices.size(); ++i)
    result.memrefIndices.push_back(globalIndices[i]);

  LLVM_DEBUG(llvm::dbgs() << "  -> dbRef: " << result.dbRefIndices.size()
                          << ", memref: " << result.memrefIndices.size()
                          << "\n");
  return result;
}

DbRewriter::LocalizedIndices
ChunkedRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                    OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG("ChunkedRewriter::localizeLinearized - using div/mod");
  LLVM_DEBUG(llvm::dbgs() << "ChunkedRewriter::localizeLinearized\n");

  // De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value col = builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  // dbRefIdx = (globalRow / chunkSize) - startChunk
  Value physChunk = builder.create<arith::DivUIOp>(loc, globalRow, chunkSize_);
  Value relChunk = builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
  result.dbRefIndices.push_back(relChunk);

  // localRow = globalRow % chunkSize
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, chunkSize_);

  // Re-linearize: localLinear = localRow * stride + col
  Value localLinear = builder.create<arith::MulIOp>(loc, localRow, stride);
  localLinear = builder.create<arith::AddIOp>(loc, localLinear, col);
  result.memrefIndices.push_back(localLinear);

  return result;
}

void ChunkedRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  LLVM_DEBUG(llvm::dbgs() << "ChunkedRewriter::rewriteDbRefUsers\n");

  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  for (Operation *refUser : refUsers) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(refUser);
    Location userLoc = refUser->getLoc();

    ValueRange elementIndices;
    if (auto load = dyn_cast<memref::LoadOp>(refUser))
      elementIndices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(refUser))
      elementIndices = store.getIndices();
    else
      continue;

    if (elementIndices.empty())
      continue;

    // Detect linearized access: single index accessing multi-element memref
    // CRITICAL FIX: For linearization stride, we need product of TRAILING dims
    // (not total elements). For [D0, D1, ...], stride = D1 * D2 * ...
    bool isLinearized = false;
    Value stride;
    if (elementIndices.size() == 1) {
      if (auto memrefType = newElementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          // Multi-dimensional memref with single index = linearized access
          if (auto staticStride = getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = builder.create<arith::ConstantIndexOp>(userLoc, *staticStride);
            }
          }
          // Note: Dynamic dimensions not handled here - caller should
          // provide stride via element_stride attribute on the acquire
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized = localizeLinearized(elementIndices[0], stride, builder, userLoc);
    } else {
      SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
      localized = localize(indices, builder, userLoc);
    }

    auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                          localized.dbRefIndices);

    if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(),
                                      newRef.getResult(),
                                      localized.memrefIndices);
      opsToRemove.insert(store);
    }
  }
  opsToRemove.insert(ref.getOperation());
}

//===----------------------------------------------------------------------===//
// ElementWiseRewriter Implementation
//===----------------------------------------------------------------------===//

ElementWiseRewriter::ElementWiseRewriter(Value elemOffset, Value elemSize,
                                         unsigned outerRank, unsigned innerRank,
                                         ValueRange oldElementSizes)
    : elemOffset_(elemOffset), elemSize_(elemSize), outerRank_(outerRank),
      innerRank_(innerRank),
      oldElementSizes_(oldElementSizes.begin(), oldElementSizes.end()) {}

DbRewriter::LocalizedIndices
ElementWiseRewriter::localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                              Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("ElementWiseRewriter::localize with " << globalIndices.size() << " indices");
  LLVM_DEBUG(llvm::dbgs() << "ElementWiseRewriter::localize with "
                          << globalIndices.size() << " indices\n");

  if (globalIndices.empty()) {
    result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  // ELEMENT-WISE: dim 0 becomes dbRef index, rest become memref indices
  // dbRefIdx = globalIdx[0] - elemOffset
  Value globalRow = globalIndices[0];
  Value localRow = builder.create<arith::SubIOp>(loc, globalRow, elemOffset_);
  result.dbRefIndices.push_back(localRow);

  // Remaining dimensions pass through to memref
  for (unsigned i = 1; i < globalIndices.size(); ++i)
    result.memrefIndices.push_back(globalIndices[i]);

  // Ensure non-empty
  if (result.memrefIndices.empty())
    result.memrefIndices.push_back(zero());

  LLVM_DEBUG(llvm::dbgs() << "  -> dbRef: " << result.dbRefIndices.size()
                          << ", memref: " << result.memrefIndices.size()
                          << "\n");
  return result;
}

DbRewriter::LocalizedIndices
ElementWiseRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                        OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG("ElementWiseRewriter::localizeLinearized - scaling offset by stride");
  LLVM_DEBUG(llvm::dbgs() << "ElementWiseRewriter::localizeLinearized\n"
                          << "  THE KEY FIX: scaling offset by stride!\n");

  // THE KEY FIX: scale offset by stride before subtracting!
  //
  // Math proof:
  //   globalLinear = (elemOffset + localRow) * stride + col
  //   localLinear  = localRow * stride + col
  //   Therefore:
  //   localLinear  = globalLinear - (elemOffset * stride)  ✓ CORRECT
  //
  // NOT: localLinear = globalLinear - elemOffset   ✗ WRONG (causes SIGSEGV!)

  Value scaledOffset = builder.create<arith::MulIOp>(loc, elemOffset_, stride);
  Value localLinear =
      builder.create<arith::SubIOp>(loc, globalLinearIndex, scaledOffset);

  // For element-wise, dbRef index = row index relative to start
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value dbRefIdx = builder.create<arith::SubIOp>(loc, globalRow, elemOffset_);

  result.dbRefIndices.push_back(dbRefIdx);
  result.memrefIndices.push_back(localLinear);

  return result;
}

void ElementWiseRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  LLVM_DEBUG(llvm::dbgs() << "ElementWiseRewriter::rewriteDbRefUsers\n");

  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  for (Operation *refUser : refUsers) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(refUser);
    Location userLoc = refUser->getLoc();

    ValueRange elementIndices;
    if (auto load = dyn_cast<memref::LoadOp>(refUser))
      elementIndices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(refUser))
      elementIndices = store.getIndices();
    else
      continue;

    if (elementIndices.empty())
      continue;

    // Detect linearized access: single index accessing multi-element memref
    // CRITICAL FIX: Use OLD element sizes for stride computation!
    // For element_sizes = [D0, D1, ...], stride = D1 * D2 * ... (TRAILING dims)
    bool isLinearized = false;
    Value stride;
    if (elementIndices.size() == 1 && oldElementSizes_.size() >= 2) {
      // Multi-dimensional old element with single index = linearized access
      // Use stored oldElementSizes_ for proper stride (static or dynamic)
      stride = getStrideValue(builder, userLoc, oldElementSizes_);
      if (stride) {
        // Check if stride > 1 (for static case)
        if (auto staticStride = getStaticStride(oldElementSizes_)) {
          if (*staticStride > 1) {
            isLinearized = true;
            LLVM_DEBUG(llvm::dbgs()
                       << "ElementWiseRewriter: linearized stride="
                       << *staticStride << " from oldElementSizes\n");
          }
        } else {
          // Dynamic stride - always use it
          isLinearized = true;
          LLVM_DEBUG(llvm::dbgs()
                     << "ElementWiseRewriter: using dynamic stride from oldElementSizes\n");
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized = localizeLinearized(elementIndices[0], stride, builder, userLoc);
    } else {
      SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
      localized = localize(indices, builder, userLoc);
    }

    auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                          localized.dbRefIndices);

    if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(),
                                      newRef.getResult(),
                                      localized.memrefIndices);
      opsToRemove.insert(store);
    }
  }
  opsToRemove.insert(ref.getOperation());
}

//===----------------------------------------------------------------------===//
// DbRewriter Factory Method
//===----------------------------------------------------------------------===//

std::unique_ptr<DbRewriter>
DbRewriter::create(bool isChunked, Value chunkSize, Value startChunk,
                   Value elemOffset, Value elemSize, unsigned newOuterRank,
                   unsigned newInnerRank, ValueRange oldElementSizes) {
  ARTS_DEBUG("DbRewriter::create - isChunked=" << isChunked
             << ", outerRank=" << newOuterRank
             << ", innerRank=" << newInnerRank);
  LLVM_DEBUG(llvm::dbgs() << "DbRewriter::create(isChunked=" << isChunked
                          << ", outerRank=" << newOuterRank
                          << ", innerRank=" << newInnerRank << ")\n");
  if (isChunked) {
    return std::make_unique<ChunkedRewriter>(chunkSize, startChunk, elemOffset,
                                             newOuterRank, newInnerRank);
  } else {
    return std::make_unique<ElementWiseRewriter>(elemOffset, elemSize,
                                                 newOuterRank, newInnerRank,
                                                 oldElementSizes);
  }
}

//===----------------------------------------------------------------------===//
// ChunkedRewriter - Mode-aware Coordinate Localization
//===----------------------------------------------------------------------===//

SmallVector<Value>
ChunkedRewriter::localizeCoordinates(ArrayRef<Value> globalIndices,
                                     ArrayRef<Value> sliceOffsets,
                                     unsigned numIndexedDims, Type elementType,
                                     OpBuilder &builder, Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  LLVM_DEBUG(llvm::dbgs() << "ChunkedRewriter::localizeCoordinates with "
                          << globalIndices.size() << " indices\n");

  // Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  // CHUNKED MODE: first sliced dimension uses div by chunkSize
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      // Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && chunkSize_) {
      // First sliced dimension: compute chunk-relative index
      // dbRefIdx = (globalIdx / chunkSize) - startChunk
      Value physChunk =
          builder.create<arith::DivUIOp>(loc, globalIdx, chunkSize_);
      Value relChunk =
          builder.create<arith::SubIOp>(loc, physChunk, startChunk_);
      result.push_back(relChunk);
      LLVM_DEBUG(llvm::dbgs() << "  Dim " << d << ": div/mod by chunkSize\n");
    } else if (d - numIndexedDims < sliceOffsets.size()) {
      // Other sliced dimensions: subtract offset
      unsigned offsetIdx = d - numIndexedDims;
      Value offset = sliceOffsets[offsetIdx];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      // Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

bool ChunkedRewriter::rebaseToAcquireView(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    OpBuilder &builder, llvm::SetVector<Operation *> &opsToRemove) {
  // Delegate to the existing standalone function for now
  // The chunked mode handling is already correct in the standalone
  // as it uses div/mod which doesn't need stride scaling
  return db::rebaseToAcquireView(op, acquire, dbPtr, elementType, builder,
                                 opsToRemove);
}

bool ChunkedRewriter::rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                                  OpBuilder &builder) {
  // Delegate to existing function - chunked mode is already handled correctly
  return db::rebaseAllUsersToAcquireView(acquire, builder);
}

//===----------------------------------------------------------------------===//
// ElementWiseRewriter - Mode-aware Coordinate Localization
// THE KEY FIX: scales offset by stride for linearized accesses!
//===----------------------------------------------------------------------===//

SmallVector<Value>
ElementWiseRewriter::localizeCoordinates(ArrayRef<Value> globalIndices,
                                         ArrayRef<Value> sliceOffsets,
                                         unsigned numIndexedDims,
                                         Type elementType, OpBuilder &builder,
                                         Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  LLVM_DEBUG(llvm::dbgs() << "ElementWiseRewriter::localizeCoordinates with "
                          << globalIndices.size() << " indices\n");

  // Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  // CRITICAL: Detect linearized access (single index, multi-element memref)
  // This is THE KEY FIX for the SIGSEGV crash!
  // CRITICAL FIX: Use OLD element sizes for stride computation!
  // For element_sizes = [D0, D1, ...], stride = D1 * D2 * ... (TRAILING dims)
  if (globalIndices.size() == 1 && !sliceOffsets.empty()) {
    Value globalLinear = globalIndices[0];
    Value offset = sliceOffsets[0];

    // Check if this needs stride scaling (old element has multiple dimensions)
    if (oldElementSizes_.size() >= 2) {
      // Multi-dimensional old element with single index = linearized access
      // Use stored oldElementSizes_ for proper stride (static or dynamic)
      Value stride = getStrideValue(builder, loc, oldElementSizes_);
      if (stride) {
        // THE KEY FIX: scale offset by stride before subtracting!
        // localLinear = globalLinear - (offset * stride)
        Value scaledOffset = builder.create<arith::MulIOp>(loc, offset, stride);
        Value localLinear =
            builder.create<arith::SubIOp>(loc, globalLinear, scaledOffset);

        if (auto staticStride = getStaticStride(oldElementSizes_)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "  LINEARIZED: scaling offset by stride=" << *staticStride
                     << " from oldElementSizes\n");
        } else {
          LLVM_DEBUG(llvm::dbgs()
                     << "  LINEARIZED: using dynamic stride from oldElementSizes\n");
        }
        result.push_back(localLinear);
        return result;
      }
    }

    // Non-linearized single index: simple subtraction
    Value local = builder.create<arith::SubIOp>(loc, globalLinear, offset);
    result.push_back(local);
    return result;
  }

  // Multi-dimensional: subtract offset from first sliced dimension only
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      // Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && !sliceOffsets.empty()) {
      // First sliced dimension: subtract element offset
      Value offset = sliceOffsets[0];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      // Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

bool ElementWiseRewriter::rebaseToAcquireView(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    OpBuilder &builder, llvm::SetVector<Operation *> &opsToRemove) {
  // For element-wise mode, we need to use our localizeCoordinates which
  // correctly handles stride scaling for linearized accesses.
  // The standalone db::rebaseToAcquireView already has the fix embedded,
  // so we delegate to it.
  return db::rebaseToAcquireView(op, acquire, dbPtr, elementType, builder,
                                 opsToRemove);
}

bool ElementWiseRewriter::rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                                      OpBuilder &builder) {
  // Delegate to existing function which has the stride scaling fix
  return db::rebaseAllUsersToAcquireView(acquire, builder);
}

///===----------------------------------------------------------------------===///
/// Coordinate Localization - Global to Local Index Transformation
///
/// DbAcquireOp specifies a view into a datablock:
///   indices: pinned dimensions (single element selection)
///   offsets: slice start position
///   sizes:   slice extent
///
/// Transformation: indexed dims -> 0, sliced dims -> global - offset
///
/// Examples:
///   Fine-grained: indices=[%i], offsets=[0]
///     db_ref[%i] -> db_ref[0]
///
///   Chunking: indices=[], offsets=[%start]
///     db_ref[%start+%j] -> db_ref[%j]
///
///   2D slice: indices=[%row], offsets=[%col_start]
///     db_ref[%row, %col] -> db_ref[0, %col - %col_start]
///===----------------------------------------------------------------------===///

/// Check if value is visible in block (for safe arithmetic generation).
static bool isVisibleIn(Value val, Block *block) {
  if (!val || !block)
    return false;
  Block *defBlock = val.isa<BlockArgument>()
                        ? val.cast<BlockArgument>().getOwner()
                        : val.getDefiningOp()->getBlock();
  for (Block *b = block; b;
       b = b->getParentOp() ? b->getParentOp()->getBlock() : nullptr)
    if (b == defBlock)
      return true;
  return false;
}

/// Check if 'value' is provably derived from 'base' via addition.
/// Returns delta if value = base + delta, nullptr otherwise.
///
/// Recognized patterns:
///   1. value == base -> delta = 0
///   2. value = arith.addi(base, delta) -> delta
///   3. value = arith.addi(delta, base) -> delta
static Value getOffsetDelta(Value value, Value base, OpBuilder &builder,
                            Location loc) {
  if (value == base)
    return builder.create<arith::ConstantIndexOp>(loc, 0);

  if (auto addOp = value.getDefiningOp<arith::AddIOp>()) {
    if (addOp.getLhs() == base)
      return addOp.getRhs();
    if (addOp.getRhs() == base)
      return addOp.getLhs();
  }
  /// Cannot prove derivation - do NOT subtract
  return nullptr;
}

/// Get indices from operation.
static ValueRange getIndices(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getIndices();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getIndices();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getIndices();
  return {};
}

/// Get memref from operation.
static Value getMemref(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getMemref();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getMemref();
  if (auto ref = dyn_cast<DbRefOp>(op))
    return ref.getSource();
  return nullptr;
}

/// Check if allocation is coarse-grained (all sizes == 1).
bool db::isCoarseGrained(DbAllocOp alloc) {
  return llvm::all_of(alloc.getSizes(), [](Value v) {
    int64_t val;
    return ValueUtils::getConstantIndex(v, val) && val == 1;
  });
}

/// Create coordinate map from acquire operation.
/// For chunked mode (promotion_mode = "chunked"), uses offset_hints for
/// element-space localization since offsets contain chunk-space indices.
ViewCoordinateMap ViewCoordinateMap::fromAcquire(DbAcquireOp acquire) {
  ViewCoordinateMap map;
  map.numIndexedDims = acquire.getIndices().size();

  /// Check for promotion mode attribute to determine offset source
  auto modeAttr =
      acquire->getAttrOfType<PromotionModeAttr>("promotion_mode");
  if (modeAttr && modeAttr.getValue() == PromotionMode::chunked) {
    /// Chunked mode: use offset_hints for element-space localization
    /// because offsets contain chunk-space indices
    map.sliceOffsets.assign(acquire.getOffsetHints().begin(),
                            acquire.getOffsetHints().end());
  } else {
    /// Non-promoted or element-wise: use offsets directly (element-space)
    map.sliceOffsets.assign(acquire.getOffsets().begin(),
                            acquire.getOffsets().end());
  }
  return map;
}

/// Create db_ref + load/store pattern from outer/inner indices.
/// File-local helper function.
static bool createDbRefPattern(Operation *op, Value dbPtr, Type elementType,
                               ArrayRef<Value> outerIndices,
                               ArrayRef<Value> innerIndices, OpBuilder &builder,
                               llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();

  /// During promotion, dbPtr may reference old allocation before replacement.
  Type resultType = elementType;
  if (!resultType || !resultType.isa<MemRefType>()) {
    if (auto dbAllocOp =
            dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(dbPtr)))
      resultType = dbAllocOp.getAllocatedElementType();
  }

  auto dbRef = builder.create<DbRefOp>(loc, resultType, dbPtr, outerIndices);
  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    auto newLoad = builder.create<memref::LoadOp>(loc, dbRef, innerIndices);
    load.replaceAllUsesWith(newLoad.getResult());
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    builder.create<memref::StoreOp>(loc, store.getValue(), dbRef, innerIndices);
  }

  opsToRemove.insert(op);
  return true;
}

/// Transform global indices to local coordinates using ViewCoordinateMap.
///
/// Transformation rules:
///   - Indexed dimensions [0, numIndexedDims): local = 0
///   - Sliced dimensions [numIndexedDims, end):
///     * If globalIdx == offset: local = 0
///     * If globalIdx = offset + delta (provable): local = delta
///     * Otherwise: local = globalIdx - offset (explicit subtraction)
///
static SmallVector<Value> localizeCoordinates(ArrayRef<Value> globalIndices,
                                              const ViewCoordinateMap &map,
                                              OpBuilder &builder,
                                              Location loc) {
  SmallVector<Value> localIndices;

  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < map.numIndexedDims) {
      /// Indexed dimension: local index is always 0
      localIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    } else {
      unsigned offsetIdx = d - map.numIndexedDims;
      if (offsetIdx < map.sliceOffsets.size()) {
        Value offset = map.sliceOffsets[offsetIdx];
        if (Value delta = getOffsetDelta(globalIdx, offset, builder, loc)) {
          localIndices.push_back(delta);
        } else if (auto divOp = globalIdx.getDefiningOp<arith::DivUIOp>()) {
          /// globalIdx is a chunk index from divui. Check if offset is already
          /// in chunk-space (e.g., from DivUIOp with same divisor) to avoid
          /// double-conversion.
          int64_t chunkSize;
          if (ValueUtils::getConstantIndex(divOp.getRhs(), chunkSize) &&
              chunkSize > 1) {
            /// Check if offset is already in chunk-space
            bool offsetAlreadyChunkSpace = false;
            if (auto offsetDivOp = offset.getDefiningOp<arith::DivUIOp>()) {
              int64_t offsetDivisor;
              if (ValueUtils::getConstantIndex(offsetDivOp.getRhs(), offsetDivisor) &&
                  offsetDivisor == chunkSize) {
                offsetAlreadyChunkSpace = true;
              }
            }

            if (offsetAlreadyChunkSpace) {
              /// Both globalIdx and offset are in chunk-space, subtract
              /// directly
              Value local =
                  builder.create<arith::SubIOp>(loc, globalIdx, offset);
              localIndices.push_back(local);
            } else {
              /// offset is in element-space, convert to chunk-space
              Value chunkSizeVal =
                  builder.create<arith::ConstantIndexOp>(loc, chunkSize);
              Value chunkOffset =
                  builder.create<arith::DivUIOp>(loc, offset, chunkSizeVal);
              Value local =
                  builder.create<arith::SubIOp>(loc, globalIdx, chunkOffset);
              localIndices.push_back(local);
            }
          } else {
            Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
            localIndices.push_back(local);
          }
        } else {
          Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
          localIndices.push_back(local);
        }
      } else {
        localIndices.push_back(globalIdx);
      }
    }
  }

  if (localIndices.empty())
    localIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return localIndices;
}

/// Rewrite memref load/store to db_ref pattern.
/// Before: load %mem[%i, %j]
/// After:  %ref = db_ref %db[%i]; load %ref[%j]  (outerCount=1)
bool db::rewriteAccessWithDbPattern(Operation *op, Value dbPtr,
                                    Type elementType, unsigned outerCount,
                                    OpBuilder &builder,
                                    llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();

  /// Split indices at outerCount: (outer[0:count], inner[count:end])
  SmallVector<Value> outerIndices, innerIndices;
  ValueRange indices = getIndices(op);
  for (auto indexed : llvm::enumerate(indices)) {
    if (indexed.index() < outerCount)
      outerIndices.push_back(indexed.value());
    else
      innerIndices.push_back(indexed.value());
  }
  if (outerIndices.empty())
    outerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (innerIndices.empty())
    innerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            builder, opsToRemove);
}

/// Rebase operation indices to acquired view's local coordinates.
/// MODE-AWARE: Uses DbRewriter to properly handle chunked vs element-wise mode.
bool db::rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                             Type elementType, OpBuilder &builder,
                             llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("db::rebaseToAcquireView - op=" << op->getName().getStringRef());

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();
  Block *insertBlock = builder.getInsertionBlock();

  /// CRITICAL FIX: Detect promotion mode and create appropriate DbRewriter
  /// This ensures ALL localization is mode-aware!
  bool isChunked = false;
  Value chunkSize, startChunk, elemOffset, elemSize;

  if (auto modeAttr = acquire->getAttrOfType<PromotionModeAttr>("promotion_mode")) {
    isChunked = (modeAttr.getValue() == PromotionMode::chunked);
  }

  /// Extract mode-specific parameters from acquire hints
  if (!acquire.getOffsetHints().empty())
    elemOffset = acquire.getOffsetHints()[0];
  if (!acquire.getSizeHints().empty())
    elemSize = acquire.getSizeHints()[0];

  /// For chunked mode, get chunkSize from sizes and compute startChunk
  if (isChunked && !acquire.getSizes().empty()) {
    chunkSize = acquire.getSizes()[0];
    if (!acquire.getOffsets().empty() && chunkSize) {
      startChunk = acquire.getOffsets()[0];
    }
  }

  /// Determine outer/inner ranks from acquire and get old element sizes
  unsigned outerRank = acquire.getSizes().size();
  unsigned innerRank = 1; // Default, will be refined below
  ValueRange oldElementSizes;

  if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
          arts::getUnderlyingDbAlloc(acquire.getSourcePtr()))) {
    innerRank = dbAllocOp.getElementSizes().size();
    oldElementSizes = dbAllocOp.getElementSizes();
    ARTS_DEBUG("db::rebaseToAcquireView: found DbAllocOp, oldElementSizes.size()="
               << oldElementSizes.size() << ", innerRank=" << innerRank);
  } else {
    ARTS_DEBUG("db::rebaseToAcquireView: NO DbAllocOp found from acquire.getSourcePtr()!");
  }

  /// Create mode-aware rewriter with old element sizes for proper stride
  auto rewriter = DbRewriter::create(isChunked, chunkSize, startChunk,
                                     elemOffset, elemSize, outerRank, innerRank,
                                     oldElementSizes);

  ARTS_DEBUG("db::rebaseToAcquireView - mode=" << (isChunked ? "chunked" : "element-wise")
             << ", outerRank=" << outerRank << ", innerRank=" << innerRank);

  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    SmallVector<Value> refIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
    if (!llvm::all_of(refIndices,
                      [&](Value v) { return isVisibleIn(v, insertBlock); }))
      return false;

    Value source = dbPtr ? dbPtr : dbRef.getSource();
    Type resultType = dbRef.getResult().getType();
    if (auto dbAllocOp =
            dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(source)))
      resultType = dbAllocOp.getAllocatedElementType();

    /// Use DbRewriter for mode-aware index localization
    /// This properly handles div/mod for chunked and stride-scaling for element-wise
    DbRewriter::LocalizedIndices localized;

    ARTS_DEBUG("db::rebaseToAcquireView (DbRefOp): refIndices.size()="
               << refIndices.size() << ", oldElementSizes.size()="
               << oldElementSizes.size());

    /// Check for linearized access (single index to multi-element memref)
    bool isLinearized = false;
    Value stride;
    if (refIndices.size() == 1) {
      /// Check element_stride attribute (set during element-wise promotion)
      if (auto strideAttr = acquire->getAttrOfType<IntegerAttr>("element_stride")) {
        int64_t strideVal = strideAttr.getInt();
        if (strideVal > 1) {
          isLinearized = true;
          stride = builder.create<arith::ConstantIndexOp>(loc, strideVal);
        }
      }
      /// CRITICAL FIX: Check OLD element sizes, not NEW result type!
      /// After element-wise promotion, NEW type is memref<f32> with stride=1.
      /// We need OLD element sizes (e.g., [4,16]) to detect linearized access.
      if (!isLinearized && !oldElementSizes.empty()) {
        if (oldElementSizes.size() >= 2) {
          if (auto staticStride = getStaticStride(oldElementSizes)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = builder.create<arith::ConstantIndexOp>(loc, *staticStride);
              ARTS_DEBUG("rebaseToAcquireView: isLinearized=true from oldElementSizes, stride="
                         << *staticStride);
            }
          } else {
            // Dynamic stride - still linearized, compute at runtime
            isLinearized = true;
            stride = getStrideValue(builder, loc, oldElementSizes);
            ARTS_DEBUG("rebaseToAcquireView: isLinearized=true with dynamic stride");
          }
        }
      }

      // Fallback: infer stride from the index expression itself (e.g., row*16)
      if (!isLinearized) {
        if (auto inferred = ValueUtils::inferConstantStride(refIndices[0], elemOffset)) {
          isLinearized = true;
          stride = builder.create<arith::ConstantIndexOp>(loc, *inferred);
          ARTS_DEBUG("rebaseToAcquireView: inferred linearized stride="
                     << *inferred << " from index expression");
        }
      }
    }

    if (isLinearized && stride) {
      localized = rewriter->localizeLinearized(refIndices[0], stride, builder, loc);
    } else {
      localized = rewriter->localize(refIndices, builder, loc);
    }

    auto newRef = builder.create<DbRefOp>(loc, resultType, source,
                                          localized.dbRefIndices);
    dbRef.replaceAllUsesWith(newRef.getResult());
    opsToRemove.insert(op);
    return true;
  }

  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  ValueRange opIndices = getIndices(op);
  if (!llvm::all_of(opIndices,
                    [&](Value v) { return isVisibleIn(v, insertBlock); }))
    return false;

  Operation *underlyingDb = arts::getUnderlyingDb(getMemref(op));
  if (underlyingDb) {
    bool allZero = llvm::all_of(opIndices, [](Value v) {
      int64_t cst;
      return ValueUtils::getConstantIndex(v, cst) && cst == 0;
    });
    if (allZero)
      return false;
  }

  SmallVector<Value> allIndices(opIndices.begin(), opIndices.end());

  /// Use DbRewriter for mode-aware index localization
  DbRewriter::LocalizedIndices localized;

  /// Check for linearized access
  bool isLinearized = false;
  Value stride;
  if (allIndices.size() == 1) {
    /// Check element_stride attribute
    if (auto strideAttr = acquire->getAttrOfType<IntegerAttr>("element_stride")) {
      int64_t strideVal = strideAttr.getInt();
      if (strideVal > 1) {
        isLinearized = true;
        stride = builder.create<arith::ConstantIndexOp>(loc, strideVal);
      }
    }
    /// CRITICAL FIX: Check OLD element sizes, not NEW element type!
    /// After element-wise promotion, NEW type is memref<f32> with stride=1.
    /// We need OLD element sizes (e.g., [4,16]) to detect linearized access.
    if (!isLinearized && !oldElementSizes.empty()) {
      if (oldElementSizes.size() >= 2) {
        if (auto staticStride = getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            stride = builder.create<arith::ConstantIndexOp>(loc, *staticStride);
            ARTS_DEBUG("rebaseToAcquireView (load/store): isLinearized=true from oldElementSizes, stride="
                       << *staticStride);
          }
        } else {
          // Dynamic stride - still linearized, compute at runtime
          isLinearized = true;
          stride = getStrideValue(builder, loc, oldElementSizes);
          ARTS_DEBUG("rebaseToAcquireView (load/store): isLinearized=true with dynamic stride");
        }
      }
    }
  }

  if (isLinearized && stride) {
    localized = rewriter->localizeLinearized(allIndices[0], stride, builder, loc);
  } else {
    localized = rewriter->localize(allIndices, builder, loc);
  }

  /// Use the properly localized indices from DbRewriter
  /// dbRefIndices for outer (db_ref), memrefIndices for inner (load/store)
  SmallVector<Value> outerIndices = localized.dbRefIndices;
  SmallVector<Value> innerIndices = localized.memrefIndices;

  if (outerIndices.empty())
    outerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (innerIndices.empty())
    innerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            builder, opsToRemove);
}

/// Rebase all users of acquired blockArg to local coordinates.
/// MODE-AWARE: Uses DbRewriter to properly handle chunked vs element-wise mode.
bool db::rebaseAllUsersToAcquireView(DbAcquireOp acquire, OpBuilder &builder) {
  ARTS_DEBUG("db::rebaseAllUsersToAcquireView - entering");

  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  Type targetType;
  for (Operation *user : blockArg.getUsers()) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }
  }

  if (!targetType) {
    targetType = blockArg.getType();
    if (auto outer = targetType.dyn_cast<MemRefType>())
      if (auto inner = outer.getElementType().dyn_cast<MemRefType>())
        targetType = inner;
  }

  if (!targetType || !targetType.isa<MemRefType>())
    return false;

  /// CRITICAL FIX: Create mode-aware DbRewriter
  /// This ensures ALL localization uses the correct mode-specific logic!
  bool isChunked = false;
  Value chunkSize, startChunk, elemOffset, elemSize;

  if (auto modeAttr = acquire->getAttrOfType<PromotionModeAttr>("promotion_mode")) {
    isChunked = (modeAttr.getValue() == PromotionMode::chunked);
  }

  /// Extract mode-specific parameters from acquire hints
  if (!acquire.getOffsetHints().empty())
    elemOffset = acquire.getOffsetHints()[0];
  if (!acquire.getSizeHints().empty())
    elemSize = acquire.getSizeHints()[0];

  /// For chunked mode, get chunkSize from sizes and compute startChunk
  if (isChunked && !acquire.getSizes().empty()) {
    chunkSize = acquire.getSizes()[0];
    if (!acquire.getOffsets().empty() && chunkSize) {
      startChunk = acquire.getOffsets()[0];
    }
  }

  /// Determine ranks and get old element sizes
  unsigned outerRank = acquire.getSizes().size();
  unsigned innerRank = 1;
  ValueRange oldElementSizes;

  if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
          arts::getUnderlyingDbAlloc(acquire.getSourcePtr()))) {
    innerRank = dbAllocOp.getElementSizes().size();
    oldElementSizes = dbAllocOp.getElementSizes();
  }

  /// Create mode-aware rewriter with old element sizes for proper stride
  auto rewriter = DbRewriter::create(isChunked, chunkSize, startChunk,
                                     elemOffset, elemSize, outerRank, innerRank,
                                     oldElementSizes);

  ARTS_DEBUG("db::rebaseAllUsersToAcquireView - mode=" << (isChunked ? "chunked" : "element-wise")
             << ", outerRank=" << outerRank << ", innerRank=" << innerRank);
  LLVM_DEBUG(llvm::dbgs() << "db::rebaseAllUsersToAcquireView: mode="
                          << (isChunked ? "chunked" : "element-wise") << "\n");

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  Type derivedType = targetType;
  if (auto dbAllocOp =
          dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAllocOp.getAllocatedElementType();

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(ref);
      Location loc = ref.getLoc();
      SmallVector<Value> refIndices(ref.getIndices().begin(),
                                    ref.getIndices().end());

      /// Use DbRewriter for mode-aware localization
      DbRewriter::LocalizedIndices localized;

      /// Check for linearized access
      bool isLinearized = false;
      Value stride;
      if (refIndices.size() == 1) {
        /// Check element_stride attribute (set during element-wise promotion)
        if (auto strideAttr = acquire->getAttrOfType<IntegerAttr>("element_stride")) {
          int64_t strideVal = strideAttr.getInt();
          if (strideVal > 1) {
            isLinearized = true;
            stride = builder.create<arith::ConstantIndexOp>(loc, strideVal);
            ARTS_DEBUG("rebaseAllUsers: using element_stride=" << strideVal);
          }
        }
        /// CRITICAL FIX: Check OLD element sizes, not NEW derived type!
        /// After element-wise promotion, NEW type is memref<f32> with stride=1.
        /// We need OLD element sizes (e.g., [4,16]) to detect linearized access.
        if (!isLinearized && !oldElementSizes.empty()) {
          if (oldElementSizes.size() >= 2) {
            if (auto staticStride = getStaticStride(oldElementSizes)) {
              if (*staticStride > 1) {
                isLinearized = true;
                stride = builder.create<arith::ConstantIndexOp>(loc, *staticStride);
                ARTS_DEBUG("rebaseAllUsers: isLinearized=true from oldElementSizes, stride="
                           << *staticStride);
              }
            } else {
              // Dynamic stride - still linearized, compute at runtime
              isLinearized = true;
              stride = getStrideValue(builder, loc, oldElementSizes);
              ARTS_DEBUG("rebaseAllUsers: isLinearized=true with dynamic stride");
            }
          }
        }

        // Fallback: infer stride from index expression if we have an offset but
        // no explicit element_stride. This catches linearized accesses like
        // (offset + i) * 16 + j for 1-D allocations.
        if (!isLinearized && elemOffset) {
          if (auto inferred = ValueUtils::inferConstantStride(refIndices[0], elemOffset)) {
            isLinearized = true;
            stride = builder.create<arith::ConstantIndexOp>(loc, *inferred);
            ARTS_DEBUG("rebaseAllUsers: inferred linearized stride="
                       << *inferred << " from index expression");
          }
        }
      }

      if (isLinearized && stride) {
        localized = rewriter->localizeLinearized(refIndices[0], stride, builder, loc);
      } else {
        localized = rewriter->localize(refIndices, builder, loc);
      }

      /// Use dbRefIndices for the new DbRefOp
      auto newRef = builder.create<DbRefOp>(loc, derivedType, blockArg,
                                            localized.dbRefIndices);
      ref.replaceAllUsesWith(newRef.getResult());
      opsToRemove.insert(ref.getOperation());
      rewritten = true;
    } else {
      if (rebaseToAcquireView(user, acquire, blockArg, derivedType, builder,
                              opsToRemove))
        rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

///===----------------------------------------------------------------------===///
/// DbAllocPromotion Implementation
///===----------------------------------------------------------------------===///

DbAllocPromotion::DbAllocPromotion(DbAllocOp oldAlloc, ValueRange newOuterSizes,
                                   ValueRange newInnerSizes,
                                   ArrayRef<DbAcquireOp> acquires,
                                   ArrayRef<Value> elementOffsets,
                                   ArrayRef<Value> elementSizes)
    : oldAlloc_(oldAlloc), newOuterSizes_(newOuterSizes),
      newInnerSizes_(newInnerSizes), acquires_(acquires),
      elementOffsets_(elementOffsets), elementSizes_(elementSizes) {
  /// Derive mode from rank comparison and element size:
  /// - Element-wise: newInnerSizes = [1] (single element per DB)
  /// - Chunked: same inner rank, size > 1 (div/mod on first index)
  /// - Otherwise: redistribute indices
  unsigned oldInnerRank = oldAlloc.getElementSizes().size();
  unsigned newInnerRank = newInnerSizes.size();

  /// Check if this is element-wise allocation (1 element per DB)
  /// This takes priority over rank comparison since 1D element-wise
  /// has the same rank but requires different index handling.
  bool isElementWise = false;
  if (newInnerRank == 1) {
    int64_t innerVal;
    if (ValueUtils::getConstantIndex(newInnerSizes[0], innerVal) && innerVal == 1)
      isElementWise = true;
  }

  /// Element-wise uses index redistribution (outer=globalIdx, inner=0)
  /// Chunked uses coordinate localization (outer=idx/chunk, inner=idx%chunk)
  isChunked_ = !isElementWise && (oldInnerRank == newInnerRank);
  chunkSize_ =
      (isChunked_ && !newInnerSizes.empty()) ? newInnerSizes[0] : Value();
}

///===----------------------------------------------------------------------===///
/// AcquireViewMap and Mode-Aware Coordinate Localization
///===----------------------------------------------------------------------===///

DbAllocPromotion::AcquireViewMap
DbAllocPromotion::AcquireViewMap::fromAcquire(DbAcquireOp acquire,
                                              bool isEdtBlockArg) {
  AcquireViewMap map;
  map.numIndexedDims = acquire.getIndices().size();
  map.sliceOffsets.assign(acquire.getOffsets().begin(),
                          acquire.getOffsets().end());
  /// Capture element-level offsets from offset_hints (element-space only)
  /// offset_hints[0] = elemOffset for localization
  map.elementOffsets.assign(acquire.getOffsetHints().begin(),
                            acquire.getOffsetHints().end());
  map.isEdtBlockArg = isEdtBlockArg;

  /// NOTE: startChunk and chunkSize are no longer stored in hints.
  /// For chunked mode, they are passed directly to rebaseEdtUsers() from
  /// rewriteAcquire() to avoid recomputation and coordinate space mixing.
  /// The chunkSize is available as a class member (chunkSize_).

  return map;
}

SmallVector<Value>
DbAllocPromotion::localizeIndices(ArrayRef<Value> globalIndices,
                                  const AcquireViewMap &map, OpBuilder &builder,
                                  Location loc) const {
  SmallVector<Value> localIndices;
  auto zero = [&]() {
    return builder.create<arith::ConstantIndexOp>(loc, 0);
  };

  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < map.numIndexedDims) {
      /// Indexed dimension: local index is always 0
      localIndices.push_back(zero());
      continue;
    }

    unsigned offsetIdx = d - map.numIndexedDims;
    if (offsetIdx >= map.sliceOffsets.size()) {
      /// No offset for this dimension, keep as-is
      localIndices.push_back(globalIdx);
      continue;
    }

    Value offset = map.sliceOffsets[offsetIdx];

    ///===------------------------------------------------------------------===///
    /// MODE-AWARE LOCALIZATION - DISJOINT Multi-Chunk Model
    ///===------------------------------------------------------------------===///
    if (isChunked_ && map.isEdtBlockArg) {
      /// CHUNKED MODE + EDT BLOCK ARG with DISJOINT model:
      /// Compute relative chunk index from global row index.
      ///
      /// db_ref index = (globalRow / chunkSize) - startChunk
      ///
      /// Example: globalRow=66, chunkSize=66, startChunk=0
      ///   physChunk = 66/66 = 1
      ///   relChunk = 1 - 0 = 1  (second acquired chunk)
      if (map.chunkSize && map.startChunk) {
        Value physChunk =
            builder.create<arith::DivUIOp>(loc, globalIdx, map.chunkSize);
        Value relChunk =
            builder.create<arith::SubIOp>(loc, physChunk, map.startChunk);
        localIndices.push_back(relChunk);
      } else {
        /// Fallback: single chunk, index is 0
        localIndices.push_back(zero());
      }
    } else {
      /// ALL OTHER CASES: Transform global index to local
      /// local = global - offset
      ///
      /// Try to recognize "offset + delta" patterns for cleaner IR
      if (Value delta = getOffsetDelta(globalIdx, offset, builder, loc)) {
        localIndices.push_back(delta);
      } else {
        /// Explicit subtraction for general case
        localIndices.push_back(
            builder.create<arith::SubIOp>(loc, globalIdx, offset));
      }
    }
  }

  if (localIndices.empty())
    localIndices.push_back(zero());

  return localIndices;
}

SmallVector<Value>
DbAllocPromotion::localizeElementIndices(ArrayRef<Value> globalIndices,
                                         const AcquireViewMap &map,
                                         OpBuilder &builder,
                                         Location loc) const {
  SmallVector<Value> localIndices;

  /// For chunked mode inside EDT with DISJOINT model:
  /// - First dimension (chunked): use MOD by chunkSize
  /// - Other dimensions: pass through unchanged
  ///
  /// DISJOINT model localization:
  ///   memref index = globalRow % chunkSize
  ///
  /// Example: globalRow=128, chunkSize=66
  ///   localRow = 128 % 66 = 62
  ///
  if (isChunked_ && map.isEdtBlockArg && map.chunkSize) {
    for (size_t i = 0; i < globalIndices.size(); ++i) {
      Value globalIdx = globalIndices[i];

      if (i == 0) {
        /// First dimension: localRow = globalRow % chunkSize
        localIndices.push_back(
            builder.create<arith::RemUIOp>(loc, globalIdx, map.chunkSize));
      } else {
        localIndices.push_back(globalIdx);
      }
    }
    return localIndices;
  }

  /// Non-chunked or no chunk info: indices unchanged
  localIndices.assign(globalIndices.begin(), globalIndices.end());
  return localIndices;
}

bool DbAllocPromotion::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder,
                                       Value startChunk) {
  ARTS_DEBUG("DbAllocPromotion::rebaseEdtUsers - isChunked=" << isChunked_);

  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

  LLVM_DEBUG(llvm::dbgs() << "DbAllocPromotion::rebaseEdtUsers - isChunked="
                          << isChunked_ << "\n");

  /// Determine element type from users or allocation
  Type targetType;
  for (Operation *user : blockArg.getUsers()) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      targetType = ref.getResult().getType();
      break;
    }
  }
  if (!targetType) {
    targetType = blockArg.getType();
    if (auto outer = targetType.dyn_cast<MemRefType>())
      if (auto inner = outer.getElementType().dyn_cast<MemRefType>())
        targetType = inner;
  }
  if (!targetType || !targetType.isa<MemRefType>())
    return false;

  Type derivedType = targetType;
  if (auto dbAlloc =
          dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAlloc.getAllocatedElementType();

  /// Get element offset from hints for element-wise mode
  Value elemOffset;
  Value elemSize;
  if (!acquire.getOffsetHints().empty())
    elemOffset = acquire.getOffsetHints()[0];
  if (!acquire.getSizeHints().empty())
    elemSize = acquire.getSizeHints()[0];

  /// Create DbRewriter based on mode - THE KEY SIMPLIFICATION
  /// All index localization logic is now in the rewriter classes
  /// Pass old element sizes for proper stride computation (static or dynamic)
  auto rewriter = DbRewriter::create(isChunked_, chunkSize_, startChunk,
                                     elemOffset, elemSize,
                                     newOuterSizes_.size(), newInnerSizes_.size(),
                                     oldAlloc_.getElementSizes());

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      /// Use DbRewriter to handle all index transformation
      /// This encapsulates chunked vs element-wise logic and linearized handling
      rewriter->rewriteDbRefUsers(ref, blockArg, derivedType, builder,
                                  opsToRemove);
      rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

FailureOr<DbAllocOp> DbAllocPromotion::apply(OpBuilder &builder) {
  ARTS_DEBUG("DbAllocPromotion::apply - isChunked=" << isChunked_
             << ", acquires=" << acquires_.size());

  if (!oldAlloc_)
    return failure();

  Location loc = oldAlloc_.getLoc();
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(oldAlloc_);

  /// 1. Create new allocation with the given sizes
  auto newAlloc = builder.create<DbAllocOp>(
      loc, oldAlloc_.getMode(), oldAlloc_.getRoute(), oldAlloc_.getAllocType(),
      oldAlloc_.getDbMode(), oldAlloc_.getElementType(), oldAlloc_.getAddress(),
      newOuterSizes_, newInnerSizes_);

  /// 2. Transfer metadata/attributes from old to new allocation
  transferAttributes(oldAlloc_, newAlloc);

  /// 3. Rewrite all acquires with their partition info
  for (unsigned i = 0; i < acquires_.size(); ++i) {
    rewriteAcquire(acquires_[i], elementOffsets_[i], elementSizes_[i], newAlloc,
                   builder);
  }

  /// 4. Collect and rewrite DbRefs outside of EDTs
  llvm::SetVector<Operation *> opsToRemove;
  SmallVector<DbRefOp> dbRefUsers;
  llvm::SmallPtrSet<Value, 8> visited;

  std::function<void(Value)> collectDbRefs = [&](Value value) {
    if (!value || !visited.insert(value).second)
      return;
    for (OpOperand &use : value.getUses()) {
      Operation *user = use.getOwner();
      if (auto ref = dyn_cast<DbRefOp>(user)) {
        if (ref.getSource() == value) {
          dbRefUsers.push_back(ref);
          collectDbRefs(ref.getResult());
        }
      }
    }
  };
  collectDbRefs(oldAlloc_.getPtr());

  for (DbRefOp ref : dbRefUsers) {
    rewriteDbRef(ref, newAlloc, builder);
    opsToRemove.insert(ref.getOperation());
  }

  /// 5. Replace all uses of old allocation with new
  oldAlloc_.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc_.getPtr().replaceAllUsesWith(newAlloc.getPtr());
  if (oldAlloc_.use_empty())
    opsToRemove.insert(oldAlloc_.getOperation());

  /// 6. Clean up removed operations
  OpRemovalManager removalMgr;
  for (Operation *op : opsToRemove)
    removalMgr.markForRemoval(op);
  removalMgr.removeAllMarked();

  return newAlloc;
}

void DbAllocPromotion::rewriteAcquire(DbAcquireOp acquire, Value elemOffset,
                                      Value elemSize, DbAllocOp newAlloc,
                                      OpBuilder &builder) {
  ARTS_DEBUG("DbAllocPromotion::rewriteAcquire - isChunked=" << isChunked_);

  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(acquire);
  Location loc = acquire.getLoc();

  /// Update source to new allocation
  acquire.getSourceGuidMutable().assign(newAlloc.getGuid());
  acquire.getSourcePtrMutable().assign(newAlloc.getPtr());

  /// Update acquire's ptr result type to match new source
  MemRefType newPtrType = newAlloc.getPtr().getType().cast<MemRefType>();
  Type oldAcqPtrType = acquire.getPtr().getType();
  if (oldAcqPtrType != newPtrType) {
    acquire.getPtr().setType(newPtrType);

    /// Also update EDT block argument type if this acquire feeds an EDT
    auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
    if (blockArg && blockArg.getType() != newPtrType)
      blockArg.setType(newPtrType);
  }

  /// Transform offset/size based on mode
  SmallVector<Value> newOffsets, newSizes;

  if (isChunked_ && chunkSize_) {
    /// CHUNKED: element-space → chunk-space
    ///
    /// DISJOINT MODEL: Physical chunks are disjoint and indexed by chunkSize_.
    /// This MUST match IndexRedistributor used for initialization:
    ///   - Chunk 0: rows [0, chunkSize_)
    ///   - Chunk 1: rows [chunkSize_, 2*chunkSize_)
    ///   - etc.
    ///
    /// For stencil patterns, a partition may span MULTIPLE physical chunks
    /// because the chunk boundary doesn't align with partition boundary.
    ///
    /// Example: chunkSize_=66, worker 1 needs rows 63-128
    ///   - startChunk = 63/66 = 0
    ///   - endChunk = 128/66 = 1
    ///   - chunkCount = 2

    /// Compute start chunk index using physical chunk size
    Value startChunk =
        builder.create<arith::DivUIOp>(loc, elemOffset, chunkSize_);

    /// Compute end position (elemOffset + elemSize - 1)
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value endPos = builder.create<arith::AddIOp>(loc, elemOffset, elemSize);
    endPos = builder.create<arith::SubIOp>(loc, endPos, one);

    /// Compute end chunk index
    Value endChunk = builder.create<arith::DivUIOp>(loc, endPos, chunkSize_);

    /// Compute chunk count = endChunk - startChunk + 1
    Value chunkCount = builder.create<arith::SubIOp>(loc, endChunk, startChunk);
    chunkCount = builder.create<arith::AddIOp>(loc, chunkCount, one);

    newOffsets.push_back(startChunk);
    newSizes.push_back(chunkCount);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);

    /// Set promotion mode attribute for downstream passes to identify chunked mode
    acquire->setAttr("promotion_mode",
        PromotionModeAttr::get(builder.getContext(), PromotionMode::chunked));

    /// Store element-space offsets in hints for ViewCoordinateMap localization
    /// offset_hints = {elemOffset} - pure element-space, no coordinate mixing
    /// size_hints = {elemSize} - pure element-space
    SmallVector<Value> offsetHints = {elemOffset};
    SmallVector<Value> sizeHints = {elemSize};
    acquire.getOffsetHintsMutable().assign(offsetHints);
    acquire.getSizeHintsMutable().assign(sizeHints);

    /// Use mode-aware rebase for chunked mode EDT users
    /// Pass startChunk directly to avoid recomputation
    rebaseEdtUsers(acquire, builder, startChunk);
  } else {
    /// ELEMENT-WISE: already in datablock space, no offset transformation
    newOffsets.push_back(elemOffset);
    newSizes.push_back(elemSize);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);
    acquire.getOffsetHintsMutable().assign(newOffsets);
    acquire.getSizeHintsMutable().assign(newSizes);

    /// Set promotion mode attribute for element-wise mode
    acquire->setAttr("promotion_mode",
        PromotionModeAttr::get(builder.getContext(), PromotionMode::element_wise));

    /// CRITICAL FIX: Store the OLD allocation's stride for linearized access!
    /// After element-wise promotion, the new element type has size=1, but
    /// linearized accesses need the ORIGINAL stride to properly scale offsets.
    ///
    /// For element_sizes = [D0, D1, D2, ...], stride = D1 * D2 * ...
    /// This is the product of TRAILING dimensions, NOT all dimensions!
    if (auto staticStride = getStaticElementStride(oldAlloc_)) {
      if (*staticStride > 1) {
        acquire->setAttr("element_stride",
            builder.getIndexAttr(*staticStride));
        LLVM_DEBUG(llvm::dbgs()
                   << "Element-wise promotion: stored stride=" << *staticStride
                   << " (trailing elementSizes product) in acquire attribute\n");
      }
    }

    /// For element-wise promotion, we need to redistribute indices between
    /// db_ref and load/store operations because the rank changed.
    /// Example: old inner rank=2 (memref<?x?xf64>), new inner rank=1
    /// (memref<?xf64>)
    ///   Old: db_ref[0] -> memref<?x?xf64>, load %ref[row,col]
    ///   New: db_ref[row] -> memref<?xf64>, load %ref[col]
    auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
    if (!blockArg)
      return;

    unsigned newOuterRank = newOuterSizes_.size();
    unsigned newInnerRank = newInnerSizes_.size();
    Type newElementType = newAlloc.getAllocatedElementType();

    llvm::SetVector<Operation *> opsToRemove;

    /// Collect all db_ref users
    SmallVector<DbRefOp> dbRefs;
    for (Operation *user : blockArg.getUsers()) {
      if (auto ref = dyn_cast<DbRefOp>(user))
        dbRefs.push_back(ref);
    }

    /// Helper to create zero index
    auto zero = [&](OpBuilder &b, Location l) {
      return b.create<arith::ConstantIndexOp>(l, 0);
    };

    /// Transform each db_ref and its load/store users
    for (DbRefOp ref : dbRefs) {
      /// Collect load/store users of this db_ref
      SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                        ref.getResult().getUsers().end());

      bool hasLoadStoreUsers = false;
      for (Operation *user : refUsers) {
        if (!isa<memref::LoadOp, memref::StoreOp>(user))
          continue;

        hasLoadStoreUsers = true;
        OpBuilder::InsertionGuard ig(builder);
        builder.setInsertionPoint(user);
        Location userLoc = user->getLoc();

        /// Get load/store indices
        ValueRange oldInner;
        if (auto load = dyn_cast<memref::LoadOp>(user))
          oldInner = load.getIndices();
        else if (auto store = dyn_cast<memref::StoreOp>(user))
          oldInner = store.getIndices();

        /// Use PartitionDescriptor for correct index localization
        /// CRITICAL FIX: For linearized memrefs, the offset must be scaled
        /// by stride before subtracting!
        PartitionDescriptor desc =
            PartitionDescriptor::forElementWise(elemOffset, elemSize);

        PartitionDescriptor::LocalizedIndices localized;
        ARTS_DEBUG("rewriteAcquire: load/store user has " << oldInner.size()
                   << " indices (1=linearized)");
        if (oldInner.size() == 1) {
          /// Single index - this is a LINEARIZED access!
          /// CRITICAL FIX: Must scale offset by stride before subtracting.
          ///
          /// For element-wise promotion, the "stride" is the total number of
          /// elements in each row of the ORIGINAL allocation (before promotion).
          /// - OLD memref<16xf32> -> stride = 16
          /// - OLD memref<16x32xf32> -> stride = 16*32 = 512
          ///
          /// BUG FIX: We MUST use OLD allocation's element sizes, NOT new ones!
          /// After element-wise promotion, newElementType is memref<1xf32> which
          /// gives stride=1 (WRONG!). The original had memref<16xf32> with stride=16.
          Value stride;

          /// CRITICAL FIX: Get stride from OLD allocation's element sizes!
          /// This is the correct stride for linearized index localization.
          ///
          /// For element_sizes = [D0, D1, D2, ...], stride = D1 * D2 * ...
          /// This is the product of TRAILING dimensions, NOT all dimensions!
          auto oldElementSizes = oldAlloc_.getElementSizes();
          ARTS_DEBUG("rewriteAcquire: oldElementSizes.size()=" << oldElementSizes.size());
          if (oldElementSizes.size() >= 2) {
            // Multi-dimensional: need stride computation
            if (auto staticStride = getStaticElementStride(oldAlloc_)) {
              if (*staticStride > 1) {
                stride = builder.create<arith::ConstantIndexOp>(userLoc, *staticStride);
                ARTS_DEBUG("Element-wise linearized: using OLD allocation stride="
                           << *staticStride << " (trailing elementSizes product)");
              }
            } else {
              // Dynamic dimensions - use helper to compute stride
              stride = getElementStrideValue(builder, userLoc, oldAlloc_);
            }
          }
          // Single dimension: stride = 1, no scaling needed

          if (stride) {
            // Use the linearized localization with stride
            // CRITICAL FIX: This scales the offset by stride before subtracting!
            localized = desc.localizeLinearized(oldInner[0], stride,
                                                newOuterRank, newInnerRank,
                                                builder, userLoc);
          } else {
            // Fallback: treat as multi-dimensional localization
            SmallVector<Value> indices(oldInner.begin(), oldInner.end());
            localized = desc.localize(indices, newOuterRank, newInnerRank,
                                      builder, userLoc);
          }
        } else {
          /// Multi-dimensional indices - use regular localize
          SmallVector<Value> indices(oldInner.begin(), oldInner.end());
          localized = desc.localize(indices, newOuterRank, newInnerRank,
                                    builder, userLoc);
        }

        /// Create new db_ref with transformed outer indices
        auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                              localized.dbRefIndices);

        /// Create new load/store with transformed inner indices
        if (auto load = dyn_cast<memref::LoadOp>(user)) {
          auto newLoad = builder.create<memref::LoadOp>(
              userLoc, newRef, localized.memrefIndices);
          load.replaceAllUsesWith(newLoad.getResult());
        } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
          builder.create<memref::StoreOp>(userLoc, store.getValue(), newRef,
                                          localized.memrefIndices);
        }
        opsToRemove.insert(user);
      }

      /// For db_refs without load/store users (or after all load/stores are
      /// replaced), we still need to update the db_ref type to match the new
      /// allocation element type. Create a replacement with correct type.
      if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
        OpBuilder::InsertionGuard ig(builder);
        builder.setInsertionPoint(ref);
        Location refLoc = ref.getLoc();

        /// Keep original indices but update result type
        SmallVector<Value> indices(ref.getIndices().begin(),
                                   ref.getIndices().end());
        if (indices.empty())
          indices.push_back(zero(builder, refLoc));

        auto newRef =
            builder.create<DbRefOp>(refLoc, newElementType, blockArg, indices);
        ref.replaceAllUsesWith(newRef.getResult());
      }

      /// Mark old db_ref for removal
      opsToRemove.insert(ref.getOperation());
    }

    /// Remove old operations
    for (Operation *op : opsToRemove)
      if (op->use_empty())
        op->erase();
  }
}

void DbAllocPromotion::rewriteDbRef(DbRefOp ref, DbAllocOp newAlloc,
                                    OpBuilder &builder) {
  Location loc = ref.getLoc();
  Type newElementType = newAlloc.getAllocatedElementType();
  Value newSource = (ref.getSource() == oldAlloc_.getPtr()) ? newAlloc.getPtr()
                                                            : ref.getSource();

  unsigned newOuterRank = newOuterSizes_.size();
  unsigned newInnerRank = newInnerSizes_.size();

  /// Collect load/store users of this db_ref
  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;

  auto zero = [&](OpBuilder &b, Location l) {
    return b.create<arith::ConstantIndexOp>(l, 0);
  };

  bool hasLoadStoreUsers = false;
  for (Operation *user : refUsers) {
    if (!isa<memref::LoadOp, memref::StoreOp>(user))
      continue;

    hasLoadStoreUsers = true;
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(user);
    Location userLoc = user->getLoc();

    /// Get load/store indices
    ValueRange loadStoreIndices;
    if (auto load = dyn_cast<memref::LoadOp>(user))
      loadStoreIndices = load.getIndices();
    else if (auto store = dyn_cast<memref::StoreOp>(user))
      loadStoreIndices = store.getIndices();

    /// Use IndexRedistributor to compute new outer/inner indices
    IndexRedistributor redistributor{newOuterRank, newInnerRank, isChunked_,
                                     chunkSize_};
    auto [newOuter, newInner] =
        redistributor.redistribute(loadStoreIndices, builder, userLoc);

    /// Create new db_ref with transformed outer indices
    auto newRef =
        builder.create<DbRefOp>(userLoc, newElementType, newSource, newOuter);

    /// Create new load/store with transformed inner indices
    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      auto newLoad = builder.create<memref::LoadOp>(userLoc, newRef, newInner);
      load.replaceAllUsesWith(newLoad.getResult());
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      builder.create<memref::StoreOp>(userLoc, store.getValue(), newRef,
                                      newInner);
    }
    opsToRemove.insert(user);
  }

  /// For db_refs without load/store users, just update the db_ref type
  if (!hasLoadStoreUsers && !ref.getResult().use_empty()) {
    OpBuilder::InsertionGuard ig(builder);
    builder.setInsertionPoint(ref);
    SmallVector<Value> indices(ref.getIndices().begin(),
                               ref.getIndices().end());
    if (indices.empty())
      indices.push_back(zero(builder, loc));
    auto newRef =
        builder.create<DbRefOp>(loc, newElementType, newSource, indices);
    ref.replaceAllUsesWith(newRef.getResult());
  }

  /// Remove old load/store operations
  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();
}
