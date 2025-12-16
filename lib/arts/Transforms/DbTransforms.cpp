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
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <cstdlib>

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
    return arts::getConstantIndex(v, val) && val == 1;
  });
}

/// Create coordinate map from acquire operation.
ViewCoordinateMap ViewCoordinateMap::fromAcquire(DbAcquireOp acquire) {
  ViewCoordinateMap map;
  map.numIndexedDims = acquire.getIndices().size();
  map.sliceOffsets.assign(acquire.getOffsets().begin(),
                          acquire.getOffsets().end());
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
          if (arts::getConstantIndex(divOp.getRhs(), chunkSize) &&
              chunkSize > 1) {
            /// Check if offset is already in chunk-space
            bool offsetAlreadyChunkSpace = false;
            if (auto offsetDivOp = offset.getDefiningOp<arith::DivUIOp>()) {
              int64_t offsetDivisor;
              if (arts::getConstantIndex(offsetDivOp.getRhs(), offsetDivisor) &&
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
bool db::rebaseToAcquireView(Operation *op, DbAcquireOp acquire, Value dbPtr,
                             Type elementType, OpBuilder &builder,
                             llvm::SetVector<Operation *> &opsToRemove) {

  ViewCoordinateMap map = ViewCoordinateMap::fromAcquire(acquire);
  unsigned expectedOuterCount = acquire.getSizes().size();

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();
  Block *insertBlock = builder.getInsertionBlock();

  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    SmallVector<Value> refIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
    if (!llvm::all_of(refIndices,
                      [&](Value v) { return isVisibleIn(v, insertBlock); }))
      return false;

    auto localIndices = localizeCoordinates(refIndices, map, builder, loc);
    Value source = dbPtr ? dbPtr : dbRef.getSource();
    Type resultType = dbRef.getResult().getType();
    if (auto dbAllocOp =
            dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(source)))
      resultType = dbAllocOp.getAllocatedElementType();
    auto newRef =
        builder.create<DbRefOp>(loc, resultType, source, localIndices);
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
      return arts::getConstantIndex(v, cst) && cst == 0;
    });
    if (allZero)
      return false;
  }

  SmallVector<Value> allIndices(opIndices.begin(), opIndices.end());
  auto localIndices = localizeCoordinates(allIndices, map, builder, loc);

  SmallVector<Value> outerIndices, innerIndices;
  for (unsigned i = 0; i < localIndices.size(); ++i) {
    if (i < expectedOuterCount)
      outerIndices.push_back(localIndices[i]);
    else
      innerIndices.push_back(localIndices[i]);
  }

  if (outerIndices.empty())
    outerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (innerIndices.empty())
    innerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            builder, opsToRemove);
}

/// Rebase all users of acquired blockArg to local coordinates.
bool db::rebaseAllUsersToAcquireView(DbAcquireOp acquire, OpBuilder &builder) {
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

  ViewCoordinateMap map = ViewCoordinateMap::fromAcquire(acquire);

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
      SmallVector<Value> refIndices(ref.getIndices().begin(),
                                    ref.getIndices().end());
      auto localIndices =
          localizeCoordinates(refIndices, map, builder, ref.getLoc());
      auto newRef = builder.create<DbRefOp>(ref.getLoc(), derivedType, blockArg,
                                            localIndices);
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
    if (arts::getConstantIndex(newInnerSizes[0], innerVal) && innerVal == 1)
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
  /// Capture element-level offsets from offset_hints
  map.elementOffsets.assign(acquire.getOffsetHints().begin(),
                            acquire.getOffsetHints().end());
  map.isEdtBlockArg = isEdtBlockArg;
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
    /// MODE-AWARE LOCALIZATION - The Critical Fix
    ///===------------------------------------------------------------------===///
    if (isChunked_ && map.isEdtBlockArg) {
      /// CHUNKED MODE + EDT BLOCK ARG:
      /// The block argument already represents the localized acquired view.
      /// db_ref indices are relative to the acquired slice, NOT global.
      /// Example: acquire chunks[1..2], db_ref[0] accesses first acquired chunk
      /// NO transformation needed - indices are already local.
      localIndices.push_back(globalIdx);
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

  /// For chunked mode inside EDT, element indices need localization
  /// using the element offset hints from the acquire.
  ///
  /// CRITICAL: Only localize dimensions that have corresponding offsets.
  /// Remaining dimensions pass through unchanged.
  ///
  /// Examples (elementOffsets, globalIndices -> result):
  ///   1D: [off], [i]           -> [i - off]
  ///   2D: [row_off], [row,col] -> [row - row_off, col]
  ///   3D: [off_i], [i,j,k]     -> [i - off_i, j, k]
  ///
  if (isChunked_ && map.isEdtBlockArg && !map.elementOffsets.empty()) {
    size_t numOffsetsProvided = map.elementOffsets.size();

    for (size_t i = 0; i < globalIndices.size(); ++i) {
      Value globalIdx = globalIndices[i];

      if (i < numOffsetsProvided) {
        /// This dimension has a corresponding element offset - localize it
        Value elementOffset = map.elementOffsets[i];

        /// Try to recognize "offset + delta" pattern for cleaner IR
        /// Example: (offset + i) - offset = i
        if (Value delta =
                getOffsetDelta(globalIdx, elementOffset, builder, loc)) {
          localIndices.push_back(delta);
        } else {
          /// Explicit subtraction: local = global - elementOffset
          localIndices.push_back(
              builder.create<arith::SubIOp>(loc, globalIdx, elementOffset));
        }
      } else {
        /// This dimension was NOT chunked - pass through unchanged
        localIndices.push_back(globalIdx);
      }
    }
    return localIndices;
  }

  /// Non-chunked or no element offsets: indices unchanged
  localIndices.assign(globalIndices.begin(), globalIndices.end());
  return localIndices;
}

bool DbAllocPromotion::rebaseEdtUsers(DbAcquireOp acquire, OpBuilder &builder) {
  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
  if (!blockArg)
    return false;

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

  /// Create mode-aware coordinate map for EDT block argument
  AcquireViewMap map =
      AcquireViewMap::fromAcquire(acquire, /*isEdtBlockArg=*/true);

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  Type derivedType = targetType;
  if (auto dbAlloc =
          dyn_cast_or_null<DbAllocOp>(arts::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAlloc.getAllocatedElementType();

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      OpBuilder::InsertionGuard IG(builder);
      builder.setInsertionPoint(ref);

      SmallVector<Value> refIndices(ref.getIndices().begin(),
                                    ref.getIndices().end());

      /// Use mode-aware localization for db_ref indices (chunk-level)
      auto localIndices =
          localizeIndices(refIndices, map, builder, ref.getLoc());

      auto newRef = builder.create<DbRefOp>(ref.getLoc(), derivedType, blockArg,
                                            localIndices);
      ref.replaceAllUsesWith(newRef.getResult());
      opsToRemove.insert(ref.getOperation());
      rewritten = true;

      /// For chunked mode: also localize memref.load/store indices on db_ref result
      /// These use element-level indices that need adjustment via offset_hints
      if (isChunked_ && !map.elementOffsets.empty()) {
        SmallVector<Operation *> refUsers(newRef.getResult().getUsers().begin(),
                                          newRef.getResult().getUsers().end());
        for (Operation *refUser : refUsers) {
          if (auto load = dyn_cast<memref::LoadOp>(refUser)) {
            OpBuilder::InsertionGuard IG2(builder);
            builder.setInsertionPoint(load);
            SmallVector<Value> loadIndices(load.getIndices().begin(),
                                           load.getIndices().end());
            auto localElemIndices = localizeElementIndices(
                loadIndices, map, builder, load.getLoc());
            auto newLoad = builder.create<memref::LoadOp>(
                load.getLoc(), newRef.getResult(), localElemIndices);
            load.replaceAllUsesWith(newLoad.getResult());
            opsToRemove.insert(load);
          }
          if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
            OpBuilder::InsertionGuard IG2(builder);
            builder.setInsertionPoint(store);
            SmallVector<Value> storeIndices(store.getIndices().begin(),
                                            store.getIndices().end());
            auto localElemIndices = localizeElementIndices(
                storeIndices, map, builder, store.getLoc());
            builder.create<memref::StoreOp>(store.getLoc(), store.getValue(),
                                            newRef.getResult(), localElemIndices);
            opsToRemove.insert(store);
          }
        }
      }
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

FailureOr<DbAllocOp> DbAllocPromotion::apply(OpBuilder &builder) {
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
    /// chunkOffset = elementOffset / chunkSize
    /// chunkCount = ceil(elementSize / chunkSize)
    Value chunkOffset =
        builder.create<arith::DivUIOp>(loc, elemOffset, chunkSize_);

    /// ceil(size/chunkSize) = (size + chunkSize - 1) / chunkSize
    Value one = builder.create<arith::ConstantIndexOp>(loc, 1);
    Value chunkMinusOne = builder.create<arith::SubIOp>(loc, chunkSize_, one);
    Value adjusted =
        builder.create<arith::AddIOp>(loc, elemSize, chunkMinusOne);
    Value chunkCount =
        builder.create<arith::DivUIOp>(loc, adjusted, chunkSize_);

    newOffsets.push_back(chunkOffset);
    newSizes.push_back(chunkCount);

    /// For chunked mode, use coordinate localization
    /// offsets/sizes: chunk-space (for acquire positioning)
    /// offset_hints/size_hints: element-space (for memref localization)
    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);

    /// Preserve element-space offsets in hints for memref index localization
    SmallVector<Value> elementHints = {elemOffset};
    SmallVector<Value> sizeHints = {elemSize};
    acquire.getOffsetHintsMutable().assign(elementHints);
    acquire.getSizeHintsMutable().assign(sizeHints);

    /// Use mode-aware rebase for chunked mode EDT users
    rebaseEdtUsers(acquire, builder);
  } else {
    /// ELEMENT-WISE: already in datablock space, no offset transformation
    newOffsets.push_back(elemOffset);
    newSizes.push_back(elemSize);

    acquire.getOffsetsMutable().assign(newOffsets);
    acquire.getSizesMutable().assign(newSizes);
    acquire.getOffsetHintsMutable().assign(newOffsets);
    acquire.getSizeHintsMutable().assign(newSizes);

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

        /// Use IndexRedistributor to compute new outer/inner indices with
        /// offset IMPORTANT: Must localize indices by subtracting elemOffset
        /// since the acquired view starts at elemOffset, not at 0.
        IndexRedistributor redistributor{newOuterRank, newInnerRank, false,
                                         Value()};
        auto [newOuter, newInner] = redistributor.redistributeWithOffset(
            oldInner, elemOffset, builder, userLoc);

        /// Create new db_ref with transformed outer indices
        auto newRef = builder.create<DbRefOp>(userLoc, newElementType, blockArg,
                                              newOuter);

        /// Create new load/store with transformed inner indices
        if (auto load = dyn_cast<memref::LoadOp>(user)) {
          auto newLoad =
              builder.create<memref::LoadOp>(userLoc, newRef, newInner);
          load.replaceAllUsesWith(newLoad.getResult());
        } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
          builder.create<memref::StoreOp>(userLoc, store.getValue(), newRef,
                                          newInner);
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
