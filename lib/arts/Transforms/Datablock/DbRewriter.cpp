///==========================================================================///
/// File: DbRewriter.cpp
///
/// Implementation of DbRewriter static methods and helper functions.
///==========================================================================///

#include "arts/Transforms/Datablock/DbRewriter.h"
#include "arts/Transforms/Datablock/DbChunkedRewriter.h"
#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/EdtUtils.h"
#include "arts/Transforms/Datablock/ViewCoordinateMap.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

namespace {

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

/// Create db_ref + load/store pattern from outer/inner indices.
/// File-local helper function.
static bool createDbRefPattern(Operation *op, Value dbPtr, Type elementType,
                               ArrayRef<Value> outerIndices,
                               ArrayRef<Value> innerIndices, ArtsCodegen &AC,
                               llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(AC.getBuilder());
  AC.setInsertionPoint(op);
  Location loc = op->getLoc();

  /// During promotion, dbPtr may reference old allocation before replacement.
  Type resultType = elementType;
  if (!resultType || !resultType.isa<MemRefType>()) {
    if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
            DatablockUtils::getUnderlyingDbAlloc(dbPtr)))
      resultType = dbAllocOp.getAllocatedElementType();
  }

  auto dbRef = AC.create<DbRefOp>(loc, resultType, dbPtr, outerIndices);
  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    auto newLoad = AC.create<memref::LoadOp>(loc, dbRef, innerIndices);
    load.replaceAllUsesWith(newLoad.getResult());
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    AC.create<memref::StoreOp>(loc, store.getValue(), dbRef, innerIndices);
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
              if (ValueUtils::getConstantIndex(offsetDivOp.getRhs(),
                                               offsetDivisor) &&
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

} // namespace

/// Rewrite memref load/store to db_ref pattern.
/// Before: load %mem[%i, %j]
/// After:  %ref = db_ref %db[%i]; load %ref[%j]  (outerCount=1)
bool DbRewriter::rewriteAccessWithDbPattern(
    Operation *op, Value dbPtr, Type elementType, unsigned outerCount,
    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove) {
  if (!isa<memref::LoadOp, memref::StoreOp>(op))
    return false;

  OpBuilder::InsertionGuard IG(AC.getBuilder());
  AC.setInsertionPoint(op);
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
    outerIndices.push_back(AC.createIndexConstant(0, loc));
  if (innerIndices.empty())
    innerIndices.push_back(AC.createIndexConstant(0, loc));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            AC, opsToRemove);
}

/// Rebase operation indices to acquired view's local coordinates.
bool DbRewriter::rebaseToAcquireView(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove) {
  OpBuilder::InsertionGuard IG(AC.getBuilder());
  AC.setInsertionPoint(op);
  Location loc = op->getLoc();
  Block *insertBlock = AC.getBuilder().getInsertionBlock();

  /// CRITICAL FIX: Detect promotion mode and create appropriate DbRewriter
  /// This ensures ALL localization is mode-aware!
  bool isChunked = false;
  Value chunkSize, startChunk, elemOffset, elemSize;

  if (auto modeAttr =
          acquire->getAttrOfType<PromotionModeAttr>("promotion_mode")) {
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
  unsigned innerRank = 1;
  ValueRange oldElementSizes;

  if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
          DatablockUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()))) {
    innerRank = dbAllocOp.getElementSizes().size();
    oldElementSizes = dbAllocOp.getElementSizes();
    ARTS_DEBUG("DbRewriter::rebaseToAcquireView: found DbAllocOp, "
               "oldElementSizes.size()="
               << oldElementSizes.size() << ", innerRank=" << innerRank);
  } else {
    ARTS_DEBUG("DbRewriter::rebaseToAcquireView: NO DbAllocOp found from "
               "acquire.getSourcePtr()!");
  }

  /// Create mode-aware rewriter with old element sizes for proper stride
  auto rewriter =
      DbRewriter::create(isChunked, chunkSize, startChunk, elemOffset, elemSize,
                         outerRank, innerRank, oldElementSizes);

  ARTS_DEBUG("DbRewriter::rebaseToAcquireView - mode="
             << (isChunked ? "chunked" : "element-wise")
             << ", outerRank=" << outerRank << ", innerRank=" << innerRank);

  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    SmallVector<Value> refIndices(dbRef.getIndices().begin(),
                                  dbRef.getIndices().end());
    if (!llvm::all_of(refIndices,
                      [&](Value v) { return isVisibleIn(v, insertBlock); }))
      return false;

    Value source = dbPtr ? dbPtr : dbRef.getSource();
    Type resultType = dbRef.getResult().getType();
    if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
            DatablockUtils::getUnderlyingDbAlloc(source)))
      resultType = dbAllocOp.getAllocatedElementType();

    /// Use DbRewriter for mode-aware index localization
    /// This properly handles div/mod for chunked and stride-scaling for
    /// element-wise
    DbRewriter::LocalizedIndices localized;

    ARTS_DEBUG("DbRewriter::rebaseToAcquireView (DbRefOp): refIndices.size()="
               << refIndices.size()
               << ", oldElementSizes.size()=" << oldElementSizes.size());

    /// Check for linearized access (single index to multi-element memref)
    bool isLinearized = false;
    Value stride;
    if (refIndices.size() == 1) {
      /// Check element_stride attribute (set during element-wise promotion)
      if (auto strideAttr =
              acquire->getAttrOfType<IntegerAttr>("element_stride")) {
        int64_t strideVal = strideAttr.getInt();
        if (strideVal > 1) {
          isLinearized = true;
          stride = AC.createIndexConstant(strideVal, loc);
        }
      }
      /// CRITICAL FIX: Check OLD element sizes, not NEW result type!
      /// After element-wise promotion, NEW type is memref<f32> with stride=1.
      /// We need OLD element sizes (e.g., [4,16]) to detect linearized access.
      if (!isLinearized && !oldElementSizes.empty()) {
        if (oldElementSizes.size() >= 2) {
          if (auto staticStride =
                  DatablockUtils::getStaticStride(oldElementSizes)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = AC.createIndexConstant(*staticStride, loc);
              ARTS_DEBUG("rebaseToAcquireView: isLinearized=true from "
                         "oldElementSizes, stride="
                         << *staticStride);
            }
          } else {
            // Dynamic stride - still linearized, compute at runtime
            isLinearized = true;
            stride = DatablockUtils::getStrideValue(AC.getBuilder(), loc,
                                                    oldElementSizes);
            ARTS_DEBUG(
                "rebaseToAcquireView: isLinearized=true with dynamic stride");
          }
        }
      }

      // Fallback: infer stride from the index expression itself (e.g., row*16)
      if (!isLinearized) {
        if (auto inferred =
                ValueUtils::inferConstantStride(refIndices[0], elemOffset)) {
          isLinearized = true;
          stride = AC.createIndexConstant(*inferred, loc);
          ARTS_DEBUG("rebaseToAcquireView: inferred linearized stride="
                     << *inferred << " from index expression");
        }
      }
    }

    if (isLinearized && stride) {
      localized = rewriter->localizeLinearized(refIndices[0], stride,
                                               AC.getBuilder(), loc);
    } else {
      localized = rewriter->localize(refIndices, AC.getBuilder(), loc);
    }

    auto newRef =
        AC.create<DbRefOp>(loc, resultType, source, localized.dbRefIndices);
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

  Operation *underlyingDb = DatablockUtils::getUnderlyingDb(getMemref(op));
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
    if (auto strideAttr =
            acquire->getAttrOfType<IntegerAttr>("element_stride")) {
      int64_t strideVal = strideAttr.getInt();
      if (strideVal > 1) {
        isLinearized = true;
        stride = AC.createIndexConstant(strideVal, loc);
      }
    }
    /// CRITICAL FIX: Check OLD element sizes, not NEW element type!
    /// After element-wise promotion, NEW type is memref<f32> with stride=1.
    /// We need OLD element sizes (e.g., [4,16]) to detect linearized access.
    if (!isLinearized && !oldElementSizes.empty()) {
      if (oldElementSizes.size() >= 2) {
        if (auto staticStride =
                DatablockUtils::getStaticStride(oldElementSizes)) {
          if (*staticStride > 1) {
            isLinearized = true;
            stride = AC.createIndexConstant(*staticStride, loc);
            ARTS_DEBUG("rebaseToAcquireView (load/store): isLinearized=true "
                       "from oldElementSizes, stride="
                       << *staticStride);
          }
        } else {
          // Dynamic stride - still linearized, compute at runtime
          isLinearized = true;
          stride = DatablockUtils::getStrideValue(AC.getBuilder(), loc,
                                                  oldElementSizes);
          ARTS_DEBUG("rebaseToAcquireView (load/store): isLinearized=true with "
                     "dynamic stride");
        }
      }
    }
  }

  if (isLinearized && stride) {
    localized = rewriter->localizeLinearized(allIndices[0], stride,
                                             AC.getBuilder(), loc);
  } else {
    localized = rewriter->localize(allIndices, AC.getBuilder(), loc);
  }

  /// Use the properly localized indices from DbRewriter
  /// dbRefIndices for outer (db_ref), memrefIndices for inner (load/store)
  SmallVector<Value> outerIndices = localized.dbRefIndices;
  SmallVector<Value> innerIndices = localized.memrefIndices;

  if (outerIndices.empty())
    outerIndices.push_back(AC.createIndexConstant(0, loc));
  if (innerIndices.empty())
    innerIndices.push_back(AC.createIndexConstant(0, loc));

  return createDbRefPattern(op, dbPtr, elementType, outerIndices, innerIndices,
                            AC, opsToRemove);
}

/// Rebase all users of acquired blockArg to local coordinates.
/// MODE-AWARE: Uses DbRewriter to properly handle chunked vs element-wise mode.
bool DbRewriter::rebaseAllUsersToAcquireView(DbAcquireOp acquire,
                                             ArtsCodegen &AC) {
  ARTS_DEBUG("DbRewriter::rebaseAllUsersToAcquireView - entering");

  auto [edt, blockArg] = EdtUtils::getEdtBlockArgumentForAcquire(acquire);
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

  if (auto modeAttr =
          acquire->getAttrOfType<PromotionModeAttr>("promotion_mode")) {
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
          DatablockUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()))) {
    innerRank = dbAllocOp.getElementSizes().size();
    oldElementSizes = dbAllocOp.getElementSizes();
  }

  /// Create mode-aware rewriter with old element sizes for proper stride
  auto rewriter =
      DbRewriter::create(isChunked, chunkSize, startChunk, elemOffset, elemSize,
                         outerRank, innerRank, oldElementSizes);

  ARTS_DEBUG("DbRewriter::rebaseAllUsersToAcquireView - mode="
             << (isChunked ? "chunked" : "element-wise")
             << ", outerRank=" << outerRank << ", innerRank=" << innerRank);
  LLVM_DEBUG(llvm::dbgs() << "DbRewriter::rebaseAllUsersToAcquireView: mode="
                          << (isChunked ? "chunked" : "element-wise") << "\n");

  SmallVector<Operation *> users(blockArg.getUsers().begin(),
                                 blockArg.getUsers().end());
  llvm::SetVector<Operation *> opsToRemove;
  bool rewritten = false;

  Type derivedType = targetType;
  if (auto dbAllocOp = dyn_cast_or_null<DbAllocOp>(
          DatablockUtils::getUnderlyingDbAlloc(blockArg)))
    derivedType = dbAllocOp.getAllocatedElementType();

  for (Operation *user : users) {
    if (auto ref = dyn_cast<DbRefOp>(user)) {
      OpBuilder::InsertionGuard IG(AC.getBuilder());
      AC.setInsertionPoint(ref);
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
        if (auto strideAttr =
                acquire->getAttrOfType<IntegerAttr>("element_stride")) {
          int64_t strideVal = strideAttr.getInt();
          if (strideVal > 1) {
            isLinearized = true;
            stride = AC.createIndexConstant(strideVal, loc);
            ARTS_DEBUG("rebaseAllUsers: using element_stride=" << strideVal);
          }
        }
        /// CRITICAL FIX: Check OLD element sizes, not NEW derived type!
        /// After element-wise promotion, NEW type is memref<f32> with stride=1.
        /// We need OLD element sizes (e.g., [4,16]) to detect linearized
        /// access.
        if (!isLinearized && !oldElementSizes.empty()) {
          if (oldElementSizes.size() >= 2) {
            if (auto staticStride =
                    DatablockUtils::getStaticStride(oldElementSizes)) {
              if (*staticStride > 1) {
                isLinearized = true;
                stride = AC.createIndexConstant(*staticStride, loc);
                ARTS_DEBUG("rebaseAllUsers: isLinearized=true from "
                           "oldElementSizes, stride="
                           << *staticStride);
              }
            } else {
              // Dynamic stride - still linearized, compute at runtime
              isLinearized = true;
              stride = DatablockUtils::getStrideValue(AC.getBuilder(), loc,
                                                      oldElementSizes);
              ARTS_DEBUG(
                  "rebaseAllUsers: isLinearized=true with dynamic stride");
            }
          }
        }

        // Fallback: infer stride from index expression if we have an offset but
        // no explicit element_stride. This catches linearized accesses like
        // (offset + i) * 16 + j for 1-D allocations.
        if (!isLinearized && elemOffset) {
          if (auto inferred =
                  ValueUtils::inferConstantStride(refIndices[0], elemOffset)) {
            isLinearized = true;
            stride = AC.createIndexConstant(*inferred, loc);
            ARTS_DEBUG("rebaseAllUsers: inferred linearized stride="
                       << *inferred << " from index expression");
          }
        }
      }

      if (isLinearized && stride) {
        localized = rewriter->localizeLinearized(refIndices[0], stride,
                                                 AC.getBuilder(), loc);
      } else {
        localized = rewriter->localize(refIndices, AC.getBuilder(), loc);
      }

      /// Use dbRefIndices for the new DbRefOp
      auto newRef = AC.create<DbRefOp>(loc, derivedType, blockArg,
                                       localized.dbRefIndices);
      ref.replaceAllUsesWith(newRef.getResult());
      opsToRemove.insert(ref.getOperation());
      rewritten = true;
    } else {
      if (DbRewriter::rebaseToAcquireView(user, acquire, blockArg, derivedType,
                                          AC, opsToRemove))
        rewritten = true;
    }
  }

  for (Operation *op : opsToRemove)
    if (op->use_empty())
      op->erase();

  return rewritten;
}

/// Factory method to create appropriate rewriter based on mode
std::unique_ptr<DbRewriter>
DbRewriter::create(bool isChunked, Value chunkSize, Value startChunk,
                   Value elemOffset, Value elemSize, unsigned newOuterRank,
                   unsigned newInnerRank, ValueRange oldElementSizes) {
  ARTS_DEBUG("DbRewriter::create - isChunked="
             << isChunked << ", outerRank=" << newOuterRank
             << ", innerRank=" << newInnerRank);
  LLVM_DEBUG(llvm::dbgs() << "DbRewriter::create(isChunked=" << isChunked
                          << ", outerRank=" << newOuterRank
                          << ", innerRank=" << newInnerRank << ")\n");
  if (isChunked) {
    return std::make_unique<DbChunkedRewriter>(chunkSize, startChunk, elemOffset,
                                             newOuterRank, newInnerRank);
  } else {
    return std::make_unique<DbElementWiseRewriter>(
        elemOffset, elemSize, newOuterRank, newInnerRank, oldElementSizes);
  }
}
