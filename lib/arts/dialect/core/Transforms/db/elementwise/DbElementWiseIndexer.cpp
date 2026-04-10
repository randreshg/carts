///==========================================================================///
/// File: DbElementWiseIndexer.cpp
///
/// Element-wise (fine-grained) index localization for datablock partitioning.
/// Each element of the partitioned dimension gets its own datablock entry.
///
/// IMPORTANT: For fine-grained mode, partition_indices are element COORDINATES.
/// Range acquires may instead use partition_offsets/sizes to describe a
/// contiguous span of elements; offsets are still element coordinates (range
/// start), not block offsets.
///
/// Example transformation (A[100][50], fine-grained on dim 0):
///
///   BEFORE (coarse allocation):
///     %db = arts.db_alloc memref<1x100x50xf64>   /// coarse: 1 block
///     arts.edt {
///       %ref = arts.db_ref %db[%c0]             /// single block
///       memref.load %ref[%i][%j]                /// global indices
///     }
///
///   AFTER (fine-grained partitioned):
///     %db = arts.db_alloc memref<100x50xf64>    /// outer=100 elements
///     arts.edt(%elemIdx) {                      /// EDT owns element %elemIdx
///       %ref = arts.db_ref %db[%elemIdx]        /// use elemOffset DIRECTLY
///       memref.load %ref[%j]                    /// inner index passed through
///     }
///
/// Key insight: element coordinates map directly to db_ref indices. For range
/// acquires, local indices are derived by subtracting the range start (no
/// division or modulo).
///
/// Equations:
///   dbRefIdx  = elemOffsets (element coordinates - use directly)
///   memrefIdx = globalIndices (inner indices - pass through)
///
/// For range acquires:
///   dbRefIdx  = globalOuter - rangeStart
///   memrefIdx = globalInner
///
/// Shared transformOps/transformDbRefUsers/transformUsesInParentRegion are
/// inherited from DbIndexerBase. ElementWise-specific behavior is provided
/// via detectLinearizedStride, handleEmptyIndices, and localizeForDbRefUser
/// overrides.
///==========================================================================///

#include "arts/dialect/core/Transforms/db/elementwise/DbElementWiseIndexer.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/Utils.h"
#include "arts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallBitVector.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Constructor
///===----------------------------------------------------------------------===///

DbElementWiseIndexer::DbElementWiseIndexer(const PartitionInfo &info,
                                           unsigned outerRank,
                                           unsigned innerRank,
                                           ValueRange oldElementSizes)
    : DbIndexerBase(info, outerRank, innerRank),
      oldElementSizes(oldElementSizes.begin(), oldElementSizes.end()) {}

///===----------------------------------------------------------------------===///
/// splitIndices: partition index matching and splitting
///===----------------------------------------------------------------------===///

LocalizedIndices DbElementWiseIndexer::splitIndices(ValueRange globalIndices,
                                                    OpBuilder &builder,
                                                    Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return arts::createZeroIndex(builder, loc); };

  if (globalIndices.empty()) {
    /// Empty indices: use single zero for dbRef and memref
    result.dbRefIndices.push_back(zero());
    /// Only add zero for memref if innerRank > 0 (element type needs indices)
    if (innerRank > 0)
      result.memrefIndices.push_back(zero());
    return result;
  }

  /// Element-wise (fine-grained) partitioning:
  /// Match EACH partition index to its corresponding global index.
  /// Localize matched dimensions (subtract offset), pass unmatched to memref.
  const auto &indices = partitionInfo.indices;
  const auto &offsets = partitionInfo.offsets;
  bool hasOuterGlobals = globalIndices.size() >= outerRank;

  /// N-dimensional generalized index matching
  unsigned numPinnedDims = indices.size();
  llvm::SmallBitVector isMatched(globalIndices.size(), false);
  SmallVector<int, 4> matchedGlobalIdx(numPinnedDims, -1);

  /// Phase 1: Match partition indices to global indices
  for (unsigned p = 0; p < numPinnedDims; ++p) {
    Value partitionIdx = indices[p];
    if (!partitionIdx)
      continue;

    for (unsigned g = 0; g < globalIndices.size(); ++g) {
      if (isMatched[g])
        continue;
      if (globalIndices[g] == partitionIdx ||
          ValueAnalysis::dependsOn(globalIndices[g], partitionIdx)) {
        matchedGlobalIdx[p] = static_cast<int>(g);
        isMatched[g] = true;
        break;
      }
    }
  }

  /// Phase 1b: Also try matching with offsets for range acquires
  SmallVector<int, 4> offsetMatchedIdx;
  for (unsigned o = 0; o < offsets.size(); ++o) {
    Value baseOffset = offsets[o];
    if (!baseOffset)
      continue;
    int foundIdx = -1;
    for (unsigned g = 0; g < globalIndices.size(); ++g) {
      if (isMatched[g])
        continue;
      if (globalIndices[g] == baseOffset ||
          ValueAnalysis::dependsOn(globalIndices[g], baseOffset)) {
        foundIdx = static_cast<int>(g);
        isMatched[g] = true;
        break;
      }
    }
    offsetMatchedIdx.push_back(foundIdx);
  }

  /// Phase 2: Build dbRefIndices from matched partition indices
  for (unsigned p = 0; p < numPinnedDims; ++p) {
    int g = matchedGlobalIdx[p];
    if (g >= 0) {
      result.dbRefIndices.push_back(globalIndices[g]);
    } else if (p < indices.size() && indices[p]) {
      result.dbRefIndices.push_back(indices[p]);
    } else {
      result.dbRefIndices.push_back(zero());
    }
  }

  /// Phase 2b: Handle offset-matched indices (range acquires need localization)
  for (unsigned o = 0; o < offsetMatchedIdx.size(); ++o) {
    int g = offsetMatchedIdx[o];
    if (g >= 0 && offsets[o]) {
      /// Localize: subtract offset from global index
      Value localIdx =
          arith::SubIOp::create(builder, loc, globalIndices[g], offsets[o]);
      result.dbRefIndices.push_back(localIdx);
    }
  }

  /// Fallback: if no partition indices matched but we have outer globals
  if (result.dbRefIndices.empty() && hasOuterGlobals) {
    for (unsigned i = 0; i < outerRank && i < globalIndices.size(); ++i) {
      if (!isMatched[i]) {
        result.dbRefIndices.push_back(globalIndices[i]);
        isMatched[i] = true;
      }
    }
  }

  /// Phase 3: Unmatched global indices go to memref, limited by innerRank
  for (unsigned g = 0; g < globalIndices.size(); ++g) {
    if (!isMatched[g]) {
      if (result.memrefIndices.size() < innerRank) {
        result.memrefIndices.push_back(globalIndices[g]);
      } else {
        ARTS_DEBUG("  Skipping unmatched index " << g << " - innerRank limit ("
                                                 << innerRank << ") reached");
      }
    }
  }

  /// Ensure at least one index for dbRef (always needed for db_ref op).
  /// Only add zero for memref if innerRank > 0 (element type needs indices).
  if (result.dbRefIndices.empty())
    result.dbRefIndices.push_back(zero());
  if (result.memrefIndices.empty() && innerRank > 0)
    result.memrefIndices.push_back(zero());

  return result;
}

///===----------------------------------------------------------------------===///
/// localize: element-wise index localization
///===----------------------------------------------------------------------===///

LocalizedIndices DbElementWiseIndexer::localize(ArrayRef<Value> globalIndices,
                                                OpBuilder &builder,
                                                Location loc) {
  ARTS_DEBUG("DbElementWiseIndexer::localize with " << globalIndices.size()
                                                    << " indices");

  /// Delegate to splitIndices which handles fine-grained partitioning
  /// by computing local indices: dbRefIdx = globalIdx - elemOffset
  LocalizedIndices result =
      splitIndices(ValueRange(globalIndices), builder, loc);

  ARTS_DEBUG("  -> dbRef: " << result.dbRefIndices.size()
                            << ", memref: " << result.memrefIndices.size());
  return result;
}

///===----------------------------------------------------------------------===///
/// localizeLinearized: offset-scaled linearized access
///===----------------------------------------------------------------------===///

LocalizedIndices
DbElementWiseIndexer::localizeLinearized(Value globalLinearIndex, Value stride,
                                         OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG(
      "DbElementWiseIndexer::localizeLinearized - scaling offset by stride");

  /// For linearized access, we only use the first element index (1D case).
  /// Multi-dimensional fine-grained uses localize() instead.
  Value elemOffset =
      partitionInfo.indices.empty() ? nullptr : partitionInfo.indices.front();
  if (!elemOffset) {
    /// No offset - return zeros
    Value zero = arts::createZeroIndex(builder, loc);
    result.dbRefIndices.push_back(zero);
    result.memrefIndices.push_back(globalLinearIndex);
    return result;
  }

  ///   globalLinear = (elemOffset + localRow) * stride + col
  ///   localLinear  = localRow * stride + col
  ///   Therefore:
  ///   localLinear  = globalLinear - (elemOffset * stride)

  Value scaledOffset = arith::MulIOp::create(builder, loc, elemOffset, stride);
  Value localLinear =
      arith::SubIOp::create(builder, loc, globalLinearIndex, scaledOffset);

  /// For element-wise, dbRef index = row index relative to start
  Value globalRow =
      arith::DivUIOp::create(builder, loc, globalLinearIndex, stride);
  Value dbRefIdx = arith::SubIOp::create(builder, loc, globalRow, elemOffset);

  result.dbRefIndices.push_back(dbRefIdx);
  result.memrefIndices.push_back(localLinear);

  return result;
}

///===----------------------------------------------------------------------===///
/// Virtual hook overrides
///===----------------------------------------------------------------------===///

std::pair<bool, Value> DbElementWiseIndexer::detectLinearizedStride(
    ValueRange indices, Type elementType, OpBuilder &builder, Location loc) {
  /// ElementWise uses oldElementSizes to detect linearized access.
  /// This differs from the Block mode which uses MemRefType shape.
  if (outerRank != 1 || indices.size() != 1 || oldElementSizes.size() < 2)
    return {false, Value()};

  /// Multi-dimensional old element with single index = linearized access
  Value stride = DbUtils::getStrideValue(builder, loc, oldElementSizes);
  if (!stride)
    return {false, Value()};

  if (auto staticStride = DbUtils::getStaticStride(oldElementSizes)) {
    if (*staticStride > 1) {
      ARTS_DEBUG("detectLinearizedStride: linearized stride=" << *staticStride);
      return {true, stride};
    }
    return {false, Value()};
  }

  /// Dynamic stride - always use it
  ARTS_DEBUG(
      "detectLinearizedStride: using dynamic stride from oldElementSizes");
  return {true, stride};
}

bool DbElementWiseIndexer::handleEmptyIndices(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  /// Handle scalar (rank-0) accesses by rewriting with db_ref[0]
  transformAccess(op, dbPtr, elementType, AC, opsToRemove);
  return true;
}

LocalizedIndices
DbElementWiseIndexer::localizeForDbRefUser(ValueRange elementIndices,
                                           Type newElementType,
                                           OpBuilder &builder, Location loc) {
  /// ElementWise uses localizeForFineGrained for DbRefOp users,
  /// which handles partition_indices matching and offset subtraction.

  /// First check for linearized access
  auto [isLinearized, stride] =
      detectLinearizedStride(elementIndices, newElementType, builder, loc);

  if (isLinearized && stride) {
    return localizeLinearized(elementIndices[0], stride, builder, loc);
  }

  /// Use fine-grained localization with partition indices and offsets
  SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
  return localizeForFineGrained(indices, partitionInfo.indices,
                                partitionInfo.offsets, builder, loc);
}

///===----------------------------------------------------------------------===///
/// localizeForFineGrained: fine-grained acquire view localization
///===----------------------------------------------------------------------===///

LocalizedIndices DbElementWiseIndexer::localizeForFineGrained(
    ValueRange globalIndices, ValueRange acquireIndices,
    ValueRange acquireOffsets, OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return arts::createZeroIndex(builder, loc); };

  ARTS_DEBUG("DbElementWiseIndexer::localizeForFineGrained with "
             << globalIndices.size() << " indices");

  if (globalIndices.empty()) {
    /// Empty indices: use single zero for dbRef
    result.dbRefIndices.push_back(zero());
    /// Only add zero for memref if innerRank > 0 (element type needs indices)
    if (innerRank > 0)
      result.memrefIndices.push_back(zero());
    return result;
  }

  /// N-dimensional generalized index matching:
  /// Match EACH partition index (acquireIndices) to its corresponding global
  /// index, then localize by subtraction. Unmatched globals go to memref.
  ///
  /// For partition_indices = [%i, %j] with access A[i][j][k]:
  ///   - Match %i -> globalIndices[0], %j -> globalIndices[1]
  ///   - dbRefIndices = [globalI - %i, globalJ - %j] = [0, 0] for
  ///   single-element
  ///   - memrefIndices = [%k]

  unsigned numPinnedDims = acquireIndices.size();
  SmallVector<int, 4> matchedGlobalIdx(numPinnedDims, -1);
  llvm::SmallBitVector isMatched(globalIndices.size(), false);

  /// Phase 1: Match each partition index to a corresponding global index
  for (unsigned p = 0; p < numPinnedDims; ++p) {
    Value partitionIdx = acquireIndices[p];

    for (unsigned g = 0; g < globalIndices.size(); ++g) {
      if (isMatched[g])
        continue;
      if (globalIndices[g] == partitionIdx ||
          ValueAnalysis::dependsOn(globalIndices[g], partitionIdx)) {
        matchedGlobalIdx[p] = static_cast<int>(g);
        isMatched[g] = true;
        break;
      }
    }
  }

  /// Phase 1b: If no acquireIndices, try matching with acquireOffsets
  if (numPinnedDims == 0 && !acquireOffsets.empty()) {
    for (unsigned o = 0; o < acquireOffsets.size(); ++o) {
      Value baseOffset = acquireOffsets[o];
      if (!baseOffset)
        continue;
      for (unsigned g = 0; g < globalIndices.size(); ++g) {
        if (isMatched[g])
          continue;
        if (globalIndices[g] == baseOffset ||
            ValueAnalysis::dependsOn(globalIndices[g], baseOffset)) {
          /// Found a match - localize this dimension
          Value localIdx =
              arith::SubIOp::create(builder, loc, globalIndices[g], baseOffset);
          result.dbRefIndices.push_back(localIdx);
          isMatched[g] = true;
          break;
        }
      }
    }
    /// Positional fallback for range acquires: when no offsets matched,
    /// split by rank with offset subtraction.
    if (result.dbRefIndices.empty() && globalIndices.size() > innerRank) {
      for (unsigned i = 0; i < outerRank && i < globalIndices.size(); ++i) {
        Value globalIdx = globalIndices[i];
        if (i < acquireOffsets.size() && acquireOffsets[i]) {
          Value localIdx =
              arith::SubIOp::create(builder, loc, globalIdx, acquireOffsets[i]);
          result.dbRefIndices.push_back(localIdx);
        } else {
          result.dbRefIndices.push_back(globalIdx);
        }
        isMatched.set(i);
      }
      ARTS_DEBUG("  Phase 1b positional fallback: " << outerRank << " dims");
    }
    /// Add unmatched globals to memref
    for (unsigned g = 0; g < globalIndices.size(); ++g) {
      if (!isMatched[g])
        result.memrefIndices.push_back(globalIndices[g]);
    }
    if (result.dbRefIndices.empty())
      result.dbRefIndices.push_back(zero());
    if (result.memrefIndices.empty() && innerRank > 0)
      result.memrefIndices.push_back(zero());
    return result;
  }

  /// Phase 2: Create localized dbRef indices for matched dimensions
  ///
  /// When value-based matching fails (e.g., partition indices from outside EDT
  /// vs. load indices computed inside EDT from different data sources), fall
  /// back to positional mapping: dimension p in partition -> dimension p in
  /// global indices, with localization by subtraction.
  ///
  /// EdtOp is NOT IsolatedFromAbove, so parent-scope partition values are
  /// accessible inside the EDT body for the subtraction.
  bool anyMatched = isMatched.any();

  for (unsigned p = 0; p < numPinnedDims; ++p) {
    int g = matchedGlobalIdx[p];
    if (g >= 0) {
      /// Value-matched: use the matched global index
      Value globalIdx = globalIndices[g];
      Value partitionIdx = acquireIndices[p];
      Value localIdx =
          arith::SubIOp::create(builder, loc, globalIdx, partitionIdx);
      result.dbRefIndices.push_back(localIdx);
    } else if (!anyMatched && p < globalIndices.size()) {
      /// Positional fallback: no values matched at all, use position p.
      /// Localize by subtracting the partition index.
      Value globalIdx = globalIndices[p];
      Value partitionIdx = acquireIndices[p];
      Value localIdx =
          arith::SubIOp::create(builder, loc, globalIdx, partitionIdx);
      result.dbRefIndices.push_back(localIdx);
      isMatched.set(p);
      ARTS_DEBUG("  Phase 2 positional fallback: dim " << p);
    } else {
      /// Partial match or no global index for this dimension
      result.dbRefIndices.push_back(zero());
    }
  }

  /// Phase 3: Add unmatched global indices to memref
  for (unsigned g = 0; g < globalIndices.size(); ++g) {
    if (!isMatched[g])
      result.memrefIndices.push_back(globalIndices[g]);
  }

  if (result.dbRefIndices.empty())
    result.dbRefIndices.push_back(zero());
  if (result.memrefIndices.empty())
    result.memrefIndices.push_back(zero());

  return result;
}

///===----------------------------------------------------------------------===///
/// transformAccess: single operation rewriting
///===----------------------------------------------------------------------===///

void DbElementWiseIndexer::transformAccess(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("transformAccess called for: " << op->getName() << " at "
                                            << op->getLoc());

  /// Handle subindex: redirect its source to db_ref result.
  /// polygeist.subindex takes a memref and returns a subview (e.g., B[0] from
  /// B[3][8]). We redirect its source to use the db_ref result, so downstream
  /// load/store operations work correctly with the datablock.
  if (auto subindex = dyn_cast<polygeist::SubIndexOp>(op)) {
    OpBuilder::InsertionGuard ig(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    auto zero = AC.createIndexConstant(0, loc);
    if (outerRank == 0) {
      /// Coarse mode: keep subindex semantics, just redirect the source.
      auto dbRef =
          AC.create<DbRefOp>(loc, elementType, dbPtr, ValueRange{zero});
      subindex.getSourceMutable().assign(dbRef.getResult());
      ARTS_DEBUG("  Redirected subindex source to db_ref (coarse mode)");
      return;
    }

    SmallVector<Value> dbRefIndices;
    /// Use the subindex index as the db_ref outer index. Pad remaining
    /// outer dimensions with zeros.
    dbRefIndices.push_back(subindex.getIndex());
    for (unsigned i = 1; i < outerRank; ++i)
      dbRefIndices.push_back(zero);

    auto dbRef = AC.create<DbRefOp>(loc, elementType, dbPtr, dbRefIndices);

    /// Replace subindex uses with db_ref result and remove the subindex.
    subindex.replaceAllUsesWith(dbRef.getResult());
    opsToRemove.insert(subindex.getOperation());
    ARTS_DEBUG("  Replaced subindex with db_ref");
    return;
  }

  auto access = DbUtils::getMemoryAccessInfo(op);
  if (!access) {
    ARTS_DEBUG("  Skipping non-load/store op: " << op->getName());
    return;
  }

  OpBuilder::InsertionGuard ig(AC.getBuilder());
  AC.setInsertionPoint(op);
  Location loc = op->getLoc();

  ValueRange indices = access->indices;
  ARTS_DEBUG("  Indices count: " << indices.size());
  LocalizedIndices localized = splitIndices(indices, AC.getBuilder(), loc);
  ARTS_DEBUG("  dbRefIndices: " << localized.dbRefIndices.size()
                                << ", memrefIndices: "
                                << localized.memrefIndices.size());

  auto dbRef =
      AC.create<DbRefOp>(loc, elementType, dbPtr, localized.dbRefIndices);

  if (access->isRead()) {
    auto load = cast<memref::LoadOp>(op);
    auto newLoad = AC.create<memref::LoadOp>(loc, dbRef.getResult(),
                                             localized.memrefIndices);
    load.replaceAllUsesWith(newLoad.getResult());
    ARTS_DEBUG("  Created new load, replaced uses");
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    AC.create<memref::StoreOp>(loc, store.getValue(), dbRef.getResult(),
                               localized.memrefIndices);
    ARTS_DEBUG("  Created new store");
  }
  opsToRemove.insert(op);
  ARTS_DEBUG("  Added to opsToRemove");
}

///===----------------------------------------------------------------------===///
/// localizeCoordinates: element-wise coordinate localization with stride
/// scaling
///===----------------------------------------------------------------------===///

SmallVector<Value> DbElementWiseIndexer::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return arts::createZeroIndex(builder, loc); };

  ARTS_DEBUG("DbElementWiseIndexer::localizeCoordinates with "
             << globalIndices.size() << " indices");

  /// Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  /// Detect linearized access
  if (globalIndices.size() == 1 && !sliceOffsets.empty()) {
    Value globalLinear = globalIndices[0];
    Value offset = sliceOffsets[0];

    /// Check if this needs stride scaling (old element has multiple dimensions)
    if (oldElementSizes.size() >= 2) {
      Value stride = DbUtils::getStrideValue(builder, loc, oldElementSizes);
      if (stride) {
        /// localLinear = globalLinear - (offset * stride)
        Value scaledOffset =
            arith::MulIOp::create(builder, loc, offset, stride);
        Value localLinear =
            arith::SubIOp::create(builder, loc, globalLinear, scaledOffset);

        if (auto staticStride = DbUtils::getStaticStride(oldElementSizes)) {
          ARTS_DEBUG(
              "  localizeCoordinates: LINEARIZED, stride=" << *staticStride);
        } else {
          ARTS_DEBUG("  localizeCoordinates: LINEARIZED, dynamic stride");
        }
        result.push_back(localLinear);
        return result;
      }
    }

    /// Non-linearized single index: simple subtraction
    Value local = arith::SubIOp::create(builder, loc, globalLinear, offset);
    result.push_back(local);
    return result;
  }

  /// Multi-dimensional: subtract offset from first sliced dimension only
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      /// Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d == numIndexedDims && !sliceOffsets.empty()) {
      /// First sliced dimension: subtract element offset
      Value offset = sliceOffsets[0];
      Value local = arith::SubIOp::create(builder, loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}
