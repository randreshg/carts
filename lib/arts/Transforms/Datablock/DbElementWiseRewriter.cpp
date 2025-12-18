///==========================================================================///
/// File: DbElementWiseRewriter.cpp
///
/// Implementation of DbElementWiseRewriter for element-wise mode index
/// localization.
///==========================================================================///

#include "arts/Transforms/Datablock/DbElementWiseRewriter.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db_transforms"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

DbElementWiseRewriter::DbElementWiseRewriter(Value elemOffset, Value elemSize,
                                         unsigned outerRank, unsigned innerRank,
                                         ValueRange oldElementSizes)
    : elemOffset_(elemOffset), elemSize_(elemSize), outerRank_(outerRank),
      innerRank_(innerRank),
      oldElementSizes_(oldElementSizes.begin(), oldElementSizes.end()) {}

DbRewriter::LocalizedIndices
DbElementWiseRewriter::localize(ArrayRef<Value> globalIndices, OpBuilder &builder,
                              Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbElementWiseRewriter::localize with " << globalIndices.size()
                                                   << " indices");
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localize with "
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
DbElementWiseRewriter::localizeLinearized(Value globalLinearIndex, Value stride,
                                        OpBuilder &builder, Location loc) {
  LocalizedIndices result;

  ARTS_DEBUG(
      "DbElementWiseRewriter::localizeLinearized - scaling offset by stride");
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localizeLinearized\n"
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

void DbElementWiseRewriter::rewriteDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::rewriteDbRefUsers\n");

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
      stride = DatablockUtils::DatablockUtils::getStrideValue(builder, userLoc,
                                                              oldElementSizes_);
      if (stride) {
        // Check if stride > 1 (for static case)
        if (auto staticStride = DatablockUtils::DatablockUtils::getStaticStride(
                oldElementSizes_)) {
          if (*staticStride > 1) {
            isLinearized = true;
            LLVM_DEBUG(llvm::dbgs()
                       << "DbElementWiseRewriter: linearized stride="
                       << *staticStride << " from oldElementSizes\n");
          }
        } else {
          // Dynamic stride - always use it
          isLinearized = true;
          LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter: using dynamic "
                                     "stride from oldElementSizes\n");
        }
      }
    }

    LocalizedIndices localized;
    if (isLinearized && stride) {
      localized =
          localizeLinearized(elementIndices[0], stride, builder, userLoc);
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

SmallVector<Value> DbElementWiseRewriter::localizeCoordinates(
    ArrayRef<Value> globalIndices, ArrayRef<Value> sliceOffsets,
    unsigned numIndexedDims, Type elementType, OpBuilder &builder,
    Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  LLVM_DEBUG(llvm::dbgs() << "DbElementWiseRewriter::localizeCoordinates with "
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
      Value stride = DatablockUtils::DatablockUtils::getStrideValue(
          builder, loc, oldElementSizes_);
      if (stride) {
        // THE KEY FIX: scale offset by stride before subtracting!
        // localLinear = globalLinear - (offset * stride)
        Value scaledOffset = builder.create<arith::MulIOp>(loc, offset, stride);
        Value localLinear =
            builder.create<arith::SubIOp>(loc, globalLinear, scaledOffset);

        if (auto staticStride = DatablockUtils::DatablockUtils::getStaticStride(
                oldElementSizes_)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "  LINEARIZED: scaling offset by stride="
                     << *staticStride << " from oldElementSizes\n");
        } else {
          LLVM_DEBUG(
              llvm::dbgs()
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

bool DbElementWiseRewriter::rebaseToAcquireViewImpl(
    Operation *op, DbAcquireOp acquire, Value dbPtr, Type elementType,
    ArtsCodegen &AC, llvm::SetVector<Operation *> &opsToRemove) {
  // For element-wise mode, we need to use our localizeCoordinates which
  // correctly handles stride scaling for linearized accesses.
  // The standalone DbRewriter::rebaseToAcquireView already has the fix
  // embedded, so we delegate to it.
  return DbRewriter::rebaseToAcquireView(op, acquire, dbPtr, elementType, AC,
                                         opsToRemove);
}

bool DbElementWiseRewriter::rebaseAllUsersToAcquireViewImpl(DbAcquireOp acquire,
                                                          ArtsCodegen &AC) {
  // Delegate to existing function which has the stride scaling fix
  return DbRewriter::rebaseAllUsersToAcquireView(acquire, AC);
}
