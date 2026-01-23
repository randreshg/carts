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
///==========================================================================///

#include "arts/Transforms/Datablock/ElementWise/DbElementWiseIndexer.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/ADT/SmallBitVector.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

DbElementWiseIndexer::DbElementWiseIndexer(const PartitionInfo &info,
                                           unsigned outerRank,
                                           unsigned innerRank,
                                           ValueRange oldElementSizes)
    : DbIndexerBase(info, outerRank, innerRank),
      oldElementSizes(oldElementSizes.begin(), oldElementSizes.end()) {}

LocalizedIndices DbElementWiseIndexer::splitIndices(ValueRange globalIndices,
                                                    OpBuilder &builder,
                                                    Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

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
          ValueUtils::dependsOn(globalIndices[g], partitionIdx)) {
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
          ValueUtils::dependsOn(globalIndices[g], baseOffset)) {
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
          builder.create<arith::SubIOp>(loc, globalIndices[g], offsets[o]);
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
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    result.dbRefIndices.push_back(zero);
    result.memrefIndices.push_back(globalLinearIndex);
    return result;
  }

  ///   globalLinear = (elemOffset + localRow) * stride + col
  ///   localLinear  = localRow * stride + col
  ///   Therefore:
  ///   localLinear  = globalLinear - (elemOffset * stride)

  Value scaledOffset = builder.create<arith::MulIOp>(loc, elemOffset, stride);
  Value localLinear =
      builder.create<arith::SubIOp>(loc, globalLinearIndex, scaledOffset);

  /// For element-wise, dbRef index = row index relative to start
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value dbRefIdx = builder.create<arith::SubIOp>(loc, globalRow, elemOffset);

  result.dbRefIndices.push_back(dbRefIdx);
  result.memrefIndices.push_back(localLinear);

  return result;
}

LocalizedIndices DbElementWiseIndexer::localizeForFineGrained(
    ValueRange globalIndices, ValueRange acquireIndices,
    ValueRange acquireOffsets, OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

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
          ValueUtils::dependsOn(globalIndices[g], partitionIdx)) {
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
            ValueUtils::dependsOn(globalIndices[g], baseOffset)) {
          /// Found a match - localize this dimension
          Value localIdx =
              builder.create<arith::SubIOp>(loc, globalIndices[g], baseOffset);
          result.dbRefIndices.push_back(localIdx);
          isMatched[g] = true;
          break;
        }
      }
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
  for (unsigned p = 0; p < numPinnedDims; ++p) {
    int g = matchedGlobalIdx[p];
    if (g >= 0) {
      Value globalIdx = globalIndices[g];
      Value partitionIdx = acquireIndices[p];
      Value localIdx =
          builder.create<arith::SubIOp>(loc, globalIdx, partitionIdx);
      result.dbRefIndices.push_back(localIdx);
    } else {
      /// No matching global found - use zero (element already selected by
      /// acquire)
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

    auto zero = AC.getBuilder().create<arith::ConstantIndexOp>(loc, 0);
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

    auto dbRef =
        AC.create<DbRefOp>(loc, elementType, dbPtr, dbRefIndices);

    /// Replace subindex uses with db_ref result and remove the subindex.
    subindex.replaceAllUsesWith(dbRef.getResult());
    opsToRemove.insert(subindex.getOperation());
    ARTS_DEBUG("  Replaced subindex with db_ref");
    return;
  }

  if (!isa<memref::LoadOp, memref::StoreOp>(op)) {
    ARTS_DEBUG("  Skipping non-load/store op: " << op->getName());
    return;
  }

  OpBuilder::InsertionGuard ig(AC.getBuilder());
  AC.setInsertionPoint(op);
  Location loc = op->getLoc();

  ValueRange indices = getIndicesFromOp(op);
  ARTS_DEBUG("  Indices count: " << indices.size());
  LocalizedIndices localized = splitIndices(indices, AC.getBuilder(), loc);
  ARTS_DEBUG("  dbRefIndices: " << localized.dbRefIndices.size()
                                << ", memrefIndices: "
                                << localized.memrefIndices.size());

  auto dbRef =
      AC.create<DbRefOp>(loc, elementType, dbPtr, localized.dbRefIndices);

  if (auto load = dyn_cast<memref::LoadOp>(op)) {
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

void DbElementWiseIndexer::transformOps(
    ArrayRef<Operation *> ops, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformOps with " << ops.size()
                                                        << " ops");

  for (Operation *op : ops) {
    OpBuilder::InsertionGuard IG(AC.getBuilder());
    AC.setInsertionPoint(op);
    Location loc = op->getLoc();

    /// Handle DbRefOp specially - rewrite its users
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      transformDbRefUsers(dbRef, dbPtr, elementType, AC.getBuilder(),
                          opsToRemove);
      continue;
    }

    /// Handle subindex: redirect its source to db_ref result.
    /// polygeist.subindex takes a memref and returns a subview (e.g., B[0] from
    /// B[3][8]). We redirect its source to use the db_ref result, so downstream
    /// load/store operations work correctly with the datablock.
    if (auto subindex = dyn_cast<polygeist::SubIndexOp>(op)) {
      auto zero = AC.getBuilder().create<arith::ConstantIndexOp>(loc, 0);

      if (outerRank == 0) {
        /// Coarse mode: keep subindex semantics, just redirect the source.
        auto dbRef =
            AC.create<DbRefOp>(loc, elementType, dbPtr, ValueRange{zero});
        subindex.getSourceMutable().assign(dbRef.getResult());
        ARTS_DEBUG("  Redirected subindex source to db_ref (coarse mode)");
        continue;
      }

      SmallVector<Value> dbRefIndices;
      /// Use the subindex index as the db_ref outer index. Pad remaining
      /// outer dimensions with zeros.
      dbRefIndices.push_back(subindex.getIndex());
      for (unsigned i = 1; i < outerRank; ++i)
        dbRefIndices.push_back(zero);

      auto dbRef =
          AC.create<DbRefOp>(loc, elementType, dbPtr, dbRefIndices);

      /// Replace subindex uses with db_ref result and remove the subindex.
      subindex.replaceAllUsesWith(dbRef.getResult());
      opsToRemove.insert(subindex.getOperation());
      ARTS_DEBUG("  Replaced subindex with db_ref (element-wise mode)");
      continue;
    }

    /// Handle load/store operations
    if (!isa<memref::LoadOp, memref::StoreOp>(op))
      continue;

    ValueRange indices = getIndicesFromOp(op);
    if (indices.empty())
      continue;

    /// Detect linearized access for multi-dimensional elements
    bool isLinearized = false;
    Value stride;
    if (outerRank == 1 && indices.size() == 1 && oldElementSizes.size() >= 2) {
      /// Multi-dimensional old element with single index = linearized access
      stride =
          DatablockUtils::getStrideValue(AC.getBuilder(), loc, oldElementSizes);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            ARTS_DEBUG("transformOps: linearized stride=" << *staticStride);
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          ARTS_DEBUG("transformOps: using dynamic stride from oldElementSizes");
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized = localizeLinearized(indices[0], stride, AC.getBuilder(), loc);
    } else {
      SmallVector<Value> indicesVec(indices.begin(), indices.end());
      localized = localize(indicesVec, AC.getBuilder(), loc);
    }

    /// Create db_ref + load/store pattern
    auto dbRef =
        AC.create<DbRefOp>(loc, elementType, dbPtr, localized.dbRefIndices);

    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      auto newLoad = AC.create<memref::LoadOp>(loc, dbRef.getResult(),
                                               localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      AC.create<memref::StoreOp>(loc, store.getValue(), dbRef.getResult(),
                                 localized.memrefIndices);
    }
    opsToRemove.insert(op);
  }
}

void DbElementWiseIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformDbRefUsers");

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

    /// Detect linearized access: single index accessing multi-element memref
    bool isLinearized = false;
    Value stride;
    if (outerRank == 1 && elementIndices.size() == 1 &&
        oldElementSizes.size() >= 2) {
      stride =
          DatablockUtils::getStrideValue(builder, userLoc, oldElementSizes);
      if (stride) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            ARTS_DEBUG(
                "transformDbRefUsers: linearized stride=" << *staticStride);
          }
        } else {
          /// Dynamic stride - always use it
          isLinearized = true;
          ARTS_DEBUG(
              "transformDbRefUsers: using dynamic stride from oldElementSizes");
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized =
          localizeLinearized(elementIndices[0], stride, builder, userLoc);
    } else {
      SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
      localized =
          localizeForFineGrained(indices, partitionInfo.indices,
                                 partitionInfo.offsets, builder, userLoc);
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

SmallVector<Value> DbElementWiseIndexer::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

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
      Value stride =
          DatablockUtils::getStrideValue(builder, loc, oldElementSizes);
      if (stride) {
        /// localLinear = globalLinear - (offset * stride)
        Value scaledOffset = builder.create<arith::MulIOp>(loc, offset, stride);
        Value localLinear =
            builder.create<arith::SubIOp>(loc, globalLinear, scaledOffset);

        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
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
    Value local = builder.create<arith::SubIOp>(loc, globalLinear, offset);
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
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

void DbElementWiseIndexer::transformUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbElementWiseIndexer::transformUsesInParentRegion");

  /// Collect users of the original allocation
  SmallVector<Operation *> users;
  for (auto &use : alloc->getUses()) {
    Operation *user = use.getOwner();
    /// Skip the DbAllocOp itself
    if (user == dbAlloc.getOperation())
      continue;
    /// Skip uses inside EDTs - those are rewritten via acquires separately
    if (user->getParentOfType<EdtOp>())
      continue;
    users.push_back(user);
  }

  if (users.empty()) {
    ARTS_DEBUG("  No users to rewrite in parent region");
    return;
  }

  ARTS_DEBUG("  Rewriting "
             << users.size()
             << " main body accesses with element-wise indexing");

  Type elementMemRefType = dbAlloc.getAllocatedElementType();

  for (Operation *op : users)
    transformAccess(op, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
