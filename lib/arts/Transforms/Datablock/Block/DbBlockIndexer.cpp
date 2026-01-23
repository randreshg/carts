///==========================================================================///
/// File: DbBlockIndexer.cpp
///
/// Block index localization for datablock partitioning.
///
/// Example transformation (N=100 rows, blockSize=25, 4 workers):
///
///   BEFORE (single allocation, global indices):
///     %db = arts.db_alloc memref<100xf64>
///     arts.edt {
///       %ref = arts.db_ref %db[0]
///       memref.load %ref[%j]                 /// global row j
///     }
///
///   AFTER (blocked, div/mod localization):
///     %db = arts.db_alloc memref<4x25xf64>   /// 4 blocks of 25 rows
///     arts.edt(%startBlock, %numBlocks) {
///       %block = %j / 25                     /// physical block
///       %relBlock = %block - %startBlock     /// worker-relative block
///       %localRow = %j % 25                  /// row within block
///       %ref = arts.db_ref %db[%relBlock]
///       memref.load %ref[%localRow]
///     }
///
/// Index localization formulas:
///   blockIdx = (globalRow / blockSize) - startBlock
///   localRow = globalRow % blockSize
///
/// For linearized access (single index into multi-dim memref):
///   1. De-linearize: globalRow = linear / stride, col = linear % stride
///   2. Apply div/mod: localRow = globalRow % blockSize
///   3. Re-linearize: localLinear = localRow * stride + col
///==========================================================================///

#include "arts/Transforms/Datablock/Block/DbBlockIndexer.h"
#include "arts/Codegen/ArtsCodegen.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/DatablockUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

DbBlockIndexer::DbBlockIndexer(const PartitionInfo &info,
                               ArrayRef<Value> startBlocks, unsigned outerRank,
                               unsigned innerRank)
    : DbIndexerBase(info, outerRank, innerRank),
      blockSizes(info.sizes.begin(), info.sizes.end()),
      startBlocks(startBlocks.begin(), startBlocks.end()) {}

LocalizedIndices DbBlockIndexer::localize(ArrayRef<Value> globalIndices,
                                          OpBuilder &builder, Location loc) {
  LocalizedIndices result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };
  auto one = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 1); };

  unsigned nPartDims = numPartitionedDims();
  ARTS_DEBUG("DbBlockIndexer::localize with "
             << globalIndices.size() << " indices, numPartitionedDims="
             << nPartDims << ", innerRank=" << innerRank
             << ", outerRank=" << outerRank);

  if (globalIndices.empty()) {
    /// For empty indices with multi-dimensional partitioning, we need
    /// nPartDims zeros for db_ref (to select partition 0,0,...) and
    /// 1 zero for the memref inner access.
    for (unsigned i = 0; i < nPartDims; ++i)
      result.dbRefIndices.push_back(zero());
    /// Ensure at least 1 index for dbRef
    if (result.dbRefIndices.empty())
      result.dbRefIndices.push_back(zero());
    result.memrefIndices.push_back(zero());
    return result;
  }

  /// N-D block partitioning: apply div/mod to each partitioned dimension
  /// For each partitioned dimension d, compute:
  ///   dbRefIdx[d]  = (globalIdx[d] / blockSize[d]) - startBlock[d]
  ///   memrefIdx[d] = globalIdx[d] % blockSize[d]
  ///
  /// However, we must respect innerRank (the element type's rank).
  /// For 2D array with 1D partitioning (e.g., memref<?x?xf64> → memref<?xf64>):
  ///   - access [i, j] with nPartDims=1, innerRank=1
  ///   - dbRefIndices = [div(i)]
  ///   - memrefIndices = [j] (NOT [rem(i), j] which would be 2 indices!)
  /// Count non-partitioned dimensions that need to go into memrefIndices
  unsigned nonPartDims = globalIndices.size() > nPartDims
                             ? globalIndices.size() - nPartDims
                             : 0;

  for (unsigned d = 0; d < nPartDims && d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];
    Value bs = (d < blockSizes.size() && blockSizes[d]) ? blockSizes[d] : one();
    bs = builder.create<arith::MaxUIOp>(loc, bs, one());

    Value sb =
        (d < startBlocks.size() && startBlocks[d]) ? startBlocks[d] : zero();

    /// Always compute full formula: relBlock = (globalIdx / bs) - startBlock
    /// The startBlock = 0 case will naturally simplify to physBlock
    Value physBlock = builder.create<arith::DivUIOp>(loc, globalIdx, bs);
    Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);

    result.dbRefIndices.push_back(relBlock);

    /// Only add rem(localIdx) if there's room after reserving for non-partitioned dims
    if (result.memrefIndices.size() + nonPartDims < innerRank) {
      Value localIdx = builder.create<arith::RemUIOp>(loc, globalIdx, bs);
      result.memrefIndices.push_back(localIdx);
      ARTS_DEBUG("    Dim " << d << ": dbRef=" << relBlock
                            << ", memref=" << localIdx);
    } else {
      ARTS_DEBUG("    Dim " << d << ": dbRef=" << relBlock
                            << " (skipping rem, innerRank=" << innerRank << ")");
    }
  }

  /// Pad dbRefIndices with zeros if globalIndices has fewer dimensions than
  /// nPartDims. This happens when accessing a lower-dimensional array with
  /// multi-dimensional partitioning.
  for (unsigned d = globalIndices.size(); d < nPartDims; ++d) {
    result.dbRefIndices.push_back(zero());
    ARTS_DEBUG("    Dim " << d << ": dbRef=0 (padded)");
  }

  /// Pass through remaining non-partitioned dimensions, limited by innerRank
  for (unsigned d = nPartDims;
       d < globalIndices.size() && result.memrefIndices.size() < innerRank;
       ++d) {
    result.memrefIndices.push_back(globalIndices[d]);
    ARTS_DEBUG("    Pass-through dim " << d << ": " << globalIndices[d]);
  }

  ARTS_DEBUG("  -> dbRef: " << result.dbRefIndices.size()
                            << ", memref: " << result.memrefIndices.size());
  return result;
}

LocalizedIndices DbBlockIndexer::localizeLinearized(Value globalLinearIndex,
                                                    Value stride,
                                                    OpBuilder &builder,
                                                    Location loc) {
  LocalizedIndices result;
  auto one = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 1); };
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbBlockIndexer::localizeLinearized - using div/mod for 2D");

  /// De-linearize: globalLinear = globalRow * stride + col
  Value globalRow =
      builder.create<arith::DivUIOp>(loc, globalLinearIndex, stride);
  Value globalCol =
      builder.create<arith::RemUIOp>(loc, globalLinearIndex, stride);

  /// Get block sizes for both dimensions
  /// For 2D blocking: bsRow for row dimension, bsCol for column dimension
  Value bsRow = (!blockSizes.empty() && blockSizes[0]) ? blockSizes[0] : one();
  bsRow = builder.create<arith::MaxUIOp>(loc, bsRow, one());

  /// Column block size: use blockSizes[1] if available, otherwise same as row
  Value bsCol =
      (blockSizes.size() > 1 && blockSizes[1]) ? blockSizes[1] : bsRow;
  bsCol = builder.create<arith::MaxUIOp>(loc, bsCol, one());

  Value sb = (!startBlocks.empty() && startBlocks[0]) ? startBlocks[0] : zero();

  /// dbRefIdx = (globalRow / blockSize) - startBlock
  Value physBlock = builder.create<arith::DivUIOp>(loc, globalRow, bsRow);
  Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
  result.dbRefIndices.push_back(relBlock);

  /// Compute local row (modulo block size)
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, bsRow);

  /// Compute local col (modulo block size)
  /// For 2D blocks, the column also needs to be localized within the block
  Value localCol = builder.create<arith::RemUIOp>(loc, globalCol, bsCol);

  /// If innerRank >= 2, return separate indices to match element type rank
  /// Otherwise, re-linearize with LOCAL stride
  if (innerRank >= 2) {
    result.memrefIndices.push_back(localRow);
    result.memrefIndices.push_back(localCol);
    ARTS_DEBUG("  innerRank=" << innerRank
                              << ", returning separate [localRow, localCol]");
  } else {
    /// Re-linearize with LOCAL stride (block size), not global stride
    /// Block memory is [bsRow x bsCol], so stride is bsCol
    Value localLinear = builder.create<arith::MulIOp>(loc, localRow, bsCol);
    localLinear = builder.create<arith::AddIOp>(loc, localLinear, localCol);
    result.memrefIndices.push_back(localLinear);
    ARTS_DEBUG("  innerRank=" << innerRank << ", re-linearized to single index");
  }

  return result;
}

void DbBlockIndexer::transformOps(ArrayRef<Operation *> ops, Value dbPtr,
                                  Type elementType, ArtsCodegen &AC,
                                  llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbBlockIndexer::transformOps with " << ops.size() << " ops");

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

      ARTS_DEBUG("  Handling subindex: elementType rank="
                 << (elementType.isa<MemRefType>()
                         ? elementType.cast<MemRefType>().getRank()
                         : 0)
                 << ", subindex result type rank="
                 << (subindex.getResult().getType().isa<MemRefType>()
                         ? subindex.getResult()
                               .getType()
                               .cast<MemRefType>()
                               .getRank()
                         : 0));

      SmallVector<Value> dbRefIndices;
      if (outerRank > 0) {
        Value globalIdx = subindex.getIndex();

        // Apply div/mod like load/store (see localize() lines 84-96)
        Value one = AC.getBuilder().create<arith::ConstantIndexOp>(loc, 1);
        Value bs = (blockSizes.size() > 0 && blockSizes[0]) ? blockSizes[0] : one;
        bs = AC.getBuilder().create<arith::MaxUIOp>(loc, bs, one);
        Value sb = (startBlocks.size() > 0 && startBlocks[0]) ? startBlocks[0] : zero;

        Value physBlock = AC.getBuilder().create<arith::DivUIOp>(loc, globalIdx, bs);
        Value relBlock = AC.getBuilder().create<arith::SubIOp>(loc, physBlock, sb);

        dbRefIndices.push_back(relBlock);  // Block-relative, not raw!

        for (unsigned i = 1; i < outerRank; ++i)
          dbRefIndices.push_back(zero);
      } else {
        /// Fallback to a single zero index if there is no outer dimension.
        dbRefIndices.push_back(zero);
      }

      auto dbRef =
          AC.create<DbRefOp>(loc, elementType, dbPtr, dbRefIndices);

      /// Replace subindex uses with db_ref result and remove the subindex.
      subindex.replaceAllUsesWith(dbRef.getResult());
      opsToRemove.insert(subindex.getOperation());
      ARTS_DEBUG("  Replaced subindex with db_ref (block mode)");
      continue;
    }

    /// Handle load/store operations
    if (!isa<memref::LoadOp, memref::StoreOp>(op))
      continue;

    ValueRange indices = getIndicesFromOp(op);
    if (indices.empty())
      continue;

    /// Detect linearized access for multi-dimensional memrefs
    bool isLinearized = false;
    Value stride;
    if (indices.size() == 1) {
      if (auto memrefType = elementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          if (auto staticStride = DatablockUtils::getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = AC.createIndexConstant(*staticStride, loc);
            }
          }
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

void DbBlockIndexer::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbBlockIndexer::transformDbRefUsers");

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
    if (elementIndices.size() == 1) {
      if (auto memrefType = newElementType.dyn_cast<MemRefType>()) {
        if (memrefType.getRank() >= 2) {
          if (auto staticStride = DatablockUtils::getStaticStride(memrefType)) {
            if (*staticStride > 1) {
              isLinearized = true;
              stride = builder.create<arith::ConstantIndexOp>(userLoc,
                                                              *staticStride);
            }
          }
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

SmallVector<Value>
DbBlockIndexer::localizeCoordinates(ArrayRef<Value> globalIndices,
                                    ArrayRef<Value> sliceOffsets,
                                    unsigned numIndexedDims, Type elementType,
                                    OpBuilder &builder, Location loc) {
  SmallVector<Value> result;
  auto zero = [&]() { return builder.create<arith::ConstantIndexOp>(loc, 0); };

  ARTS_DEBUG("DbBlockIndexer::localizeCoordinates with " << globalIndices.size()
                                                         << " indices");

  /// Handle empty case
  if (globalIndices.empty()) {
    result.push_back(zero());
    return result;
  }

  /// BLOCK MODE: partitioned dimensions use div by blockSize
  unsigned nPartDims = numPartitionedDims();
  for (unsigned d = 0; d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];

    if (d < numIndexedDims) {
      /// Indexed dimension: local index is always 0
      result.push_back(zero());
    } else if (d < numIndexedDims + nPartDims &&
               d - numIndexedDims < blockSizes.size()) {
      /// Partitioned dimension: compute block-relative index
      unsigned partIdx = d - numIndexedDims;
      Value bs = blockSizes[partIdx];
      Value sb = (partIdx < startBlocks.size()) ? startBlocks[partIdx] : zero();
      if (bs) {
        /// dbRefIdx = (globalIdx / blockSize) - startBlock
        Value physBlock = builder.create<arith::DivUIOp>(loc, globalIdx, bs);
        Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
        result.push_back(relBlock);
        ARTS_DEBUG("  Dim " << d << ": div/mod by blockSize[" << partIdx
                            << "]");
      } else {
        result.push_back(globalIdx);
      }
    } else if (d - numIndexedDims < sliceOffsets.size()) {
      /// Other sliced dimensions: subtract offset
      unsigned offsetIdx = d - numIndexedDims;
      Value offset = sliceOffsets[offsetIdx];
      Value local = builder.create<arith::SubIOp>(loc, globalIdx, offset);
      result.push_back(local);
    } else {
      /// Pass through unchanged
      result.push_back(globalIdx);
    }
  }

  return result;
}

void DbBlockIndexer::transformUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbBlockIndexer::transformUsesInParentRegion");

  /// Collect users of the original allocation (excluding DbAllocOp and EDT
  /// uses)
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

  ARTS_DEBUG("  Rewriting " << users.size()
                            << " main body accesses with block indexing");

  /// Use transformOps with the collected users - block div/mod transformation
  Type elementMemRefType = dbAlloc.getAllocatedElementType();
  transformOps(users, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
