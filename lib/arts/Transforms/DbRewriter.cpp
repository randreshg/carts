///===----------------------------------------------------------------------===///
// DbRewriter.cpp - Automated Datablock Use Transformation Implementation
///===----------------------------------------------------------------------===///

#include "arts/Transforms/DbRewriter.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "db-rewriter"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(db_rewriter);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// Statistics
///===----------------------------------------------------------------------===///

void DbRewriter::Statistics::print(llvm::raw_ostream &os) const {
  os << "DbRewriter Statistics:\n"
     << "  Loads transformed: " << loadsTransformed << "\n"
     << "  Stores transformed: " << storesTransformed << "\n"
     << "  DbRefs updated: " << dbRefsUpdated << "\n"
     << "  Acquires updated: " << acquiresUpdated << "\n"
     << "  Operations removed: " << operationsRemoved << "\n";
}

///===----------------------------------------------------------------------===///
// Construction
///===----------------------------------------------------------------------===///

DbRewriter::DbRewriter(MLIRContext *context, Config config)
    : context_(context), config_(config), builder_(context) {}

///===----------------------------------------------------------------------===///
// Main entry point
///===----------------------------------------------------------------------===///

LogicalResult DbRewriter::rewriteAllUses(DbAllocOp oldAlloc,
                                         DbAllocOp newAlloc) {
  oldAlloc_ = oldAlloc;
  newAlloc_ = newAlloc;

  /// Step 1: Analyze structural differences
  diff_ = analyzeChanges(oldAlloc, newAlloc);

  ARTS_INFO("DbRewriter: Analyzing transformation\n"
            << "  - Old alloc: sizes=" << oldAlloc.getSizes().size()
            << ", elemSizes=" << oldAlloc.getElementSizes().size() << "\n"
            << "  - New alloc: sizes=" << newAlloc.getSizes().size()
            << ", elemSizes=" << newAlloc.getElementSizes().size());

  switch (diff_.changeType) {
  case StructuralDiff::NoChange:
    ARTS_INFO("  - Change type: NoChange");
    break;
  case StructuralDiff::Promotion:
    ARTS_INFO("  - Change type: Promotion (" << diff_.promotedDims << " dims)");
    break;
  case StructuralDiff::Chunking:
    ARTS_INFO("  - Change type: Chunking");
    break;
  case StructuralDiff::Refinement:
    ARTS_INFO("  - Change type: Refinement");
    break;
  case StructuralDiff::Reshape:
    ARTS_INFO("  - Change type: Reshape");
    break;
  }

  /// Step 2: Configure index transformer if needed
  if (diff_.needsIndexTransform) {
    configureIndexTransformer(diff_);
  }

  /// Step 3: Collect all uses recursively
  SmallPtrSet<Value, 8> visited;
  UseSet uses = collectUsesRecursive(oldAlloc.getPtr(), visited);

  ARTS_INFO("  - Found " << uses.size() << " uses\n"
                         << "    - Loads: " << uses.loads.size() << "\n"
                         << "    - Stores: " << uses.stores.size() << "\n"
                         << "    - DbRefs: " << uses.dbRefs.size() << "\n"
                         << "    - Acquires: " << uses.acquires.size());

  if (uses.empty()) {
    ARTS_INFO("  - No uses found, replacing allocation directly");
    /// No uses - just replace the allocation
    oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
    oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
    if (config_.removeDeadOps) {
      opsToRemove_.insert(oldAlloc.getOperation());
    }
    return success();
  }

  /// Step 4: Transform each use type
  ARTS_INFO("  - Transforming uses...");

  for (auto load : uses.loads) {
    if (failed(transformLoad(load, diff_)))
      return failure();
    stats_.loadsTransformed++;
  }

  for (auto store : uses.stores) {
    if (failed(transformStore(store, diff_)))
      return failure();
    stats_.storesTransformed++;
  }

  for (auto dbRef : uses.dbRefs) {
    if (failed(transformDbRef(dbRef, diff_)))
      return failure();
    stats_.dbRefsUpdated++;
  }

  for (auto acquire : uses.acquires) {
    if (failed(transformDbAcquire(acquire, diff_)))
      return failure();
    stats_.acquiresUpdated++;
  }

  ARTS_INFO("  - Transformed: " << stats_.loadsTransformed << " loads, "
                                << stats_.storesTransformed << " stores, "
                                << stats_.dbRefsUpdated << " dbRefs, "
                                << stats_.acquiresUpdated << " acquires");

  /// Step 5: Replace GUID and pointer uses
  oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());

  /// Step 6: Cleanup
  if (config_.removeDeadOps) {
    opsToRemove_.insert(oldAlloc.getOperation());
    cleanup();
    ARTS_INFO("  - Removed " << stats_.operationsRemoved << " operations");
  }

  /// Step 7: Verify
  if (config_.preserveSemantics) {
    if (failed(verify())) {
      ARTS_INFO("  - Verification failed!");
      return failure();
    }
    ARTS_INFO("  - Verification passed");
  }

  ARTS_INFO("  - DbRewriter: Transformation complete!");
  return success();
}

///===----------------------------------------------------------------------===///
// Analysis
///===----------------------------------------------------------------------===///

DbRewriter::StructuralDiff DbRewriter::analyzeChanges(DbAllocOp oldAlloc,
                                                      DbAllocOp newAlloc) {
  StructuralDiff diff;

  ValueRange oldSizes = oldAlloc.getSizes();
  ValueRange newSizes = newAlloc.getSizes();
  ValueRange oldElemSizes = oldAlloc.getElementSizes();
  ValueRange newElemSizes = newAlloc.getElementSizes();

  diff.oldOuterRank = oldSizes.size();
  diff.newOuterRank = newSizes.size();

  /// Case 1: Identical structure - no change needed
  bool sizesMatch = (oldSizes.size() == newSizes.size());
  bool elemSizesMatch = (oldElemSizes.size() == newElemSizes.size());

  if (sizesMatch && elemSizesMatch) {
    /// Check if values are actually the same
    bool allMatch = true;
    for (size_t i = 0; i < oldSizes.size(); ++i) {
      int64_t oldVal, newVal;
      if (arts::getConstantIndex(oldSizes[i], oldVal) &&
          arts::getConstantIndex(newSizes[i], newVal)) {
        if (oldVal != newVal) {
          allMatch = false;
          break;
        }
      }
    }
    if (allMatch) {
      diff.changeType = StructuralDiff::NoChange;
      return diff;
    }
  }

  /// Case 2: Promotion (dimensions moved from sizes to elementSizes)
  if (newSizes.size() < oldSizes.size() &&
      newElemSizes.size() > oldElemSizes.size()) {
    diff.changeType = StructuralDiff::Promotion;
    diff.promotedDims = oldSizes.size() - newSizes.size();
    diff.needsIndexTransform = true;
    return diff;
  }

  /// Case 3: Chunking (array divided into chunks)
  /// Detect pattern: oldSizes=[1], newSizes=[N, M], oldElemSizes=[N*chunk,
  /// M*chunk]
  bool wasCoarseGrained = (oldSizes.size() == 1);
  if (wasCoarseGrained) {
    int64_t oldSize;
    if (arts::getConstantIndex(oldSizes[0], oldSize) && oldSize == 1) {
      diff.changeType = StructuralDiff::Chunking;
      /// Extract chunk sizes from newElemSizes
      diff.chunkSizes.assign(newElemSizes.begin(), newElemSizes.end());
      diff.needsIndexTransform = true;
      return diff;
    }
  }

  /// Case 4: Refinement (same rank, different values)
  if (oldSizes.size() == newSizes.size()) {
    diff.changeType = StructuralDiff::Refinement;
    /// May need bounds checking but not index transformation
    return diff;
  }

  /// Case 5: Complete reshape - unknown pattern
  diff.changeType = StructuralDiff::Reshape;
  diff.needsIndexTransform = true;
  return diff;
}

void DbRewriter::configureIndexTransformer(const StructuralDiff &diff) {
  //   indexTransformer_ = std::make_unique<IndexTransformer>(
  //       IndexTransformer::Strategy::Identity, builder_, oldAlloc_.getLoc());

  //   switch (diff.changeType) {
  //   case StructuralDiff::Promotion:
  //     indexTransformer_ = std::make_unique<IndexTransformer>(
  //         IndexTransformer::Strategy::Promotion, builder_,
  //         oldAlloc_.getLoc());
  //     indexTransformer_->setPromotionCount(diff.promotedDims);
  //     break;

  //   case StructuralDiff::Chunking:
  //     indexTransformer_ = std::make_unique<IndexTransformer>(
  //         IndexTransformer::Strategy::ChunkBased, builder_,
  //         oldAlloc_.getLoc());
  //     indexTransformer_->setChunkSizes(diff.chunkSizes);
  //     break;

  //   default:
  //     break;
  //   }
}

///===----------------------------------------------------------------------===///
// Use collection
///===----------------------------------------------------------------------===///

DbRewriter::UseSet
DbRewriter::collectUsesRecursive(Value source, SmallPtrSet<Value, 8> &visited) {
  UseSet uses;

  if (!source || !visited.insert(source).second)
    return uses;

  for (auto &use : source.getUses()) {
    Operation *user = use.getOwner();

    if (auto load = dyn_cast<memref::LoadOp>(user)) {
      if (load.getMemref() == source)
        uses.loads.push_back(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
      if (store.getMemref() == source)
        uses.stores.push_back(store);
    } else if (auto dbRef = dyn_cast<DbRefOp>(user)) {
      if (dbRef.getSource() == source) {
        uses.dbRefs.push_back(dbRef);
        /// Recursively collect uses of the db_ref result
        UseSet nestedUses = collectUsesRecursive(dbRef.getResult(), visited);
        uses.loads.append(nestedUses.loads.begin(), nestedUses.loads.end());
        uses.stores.append(nestedUses.stores.begin(), nestedUses.stores.end());
        uses.dbRefs.append(nestedUses.dbRefs.begin(), nestedUses.dbRefs.end());
      }
    } else if (auto acquire = dyn_cast<DbAcquireOp>(user)) {
      uses.acquires.push_back(acquire);
      /// Follow through EDT block arguments
      auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acquire);
      if (edt && blockArg) {
        UseSet edtUses = collectUsesRecursive(blockArg, visited);
        uses.loads.append(edtUses.loads.begin(), edtUses.loads.end());
        uses.stores.append(edtUses.stores.begin(), edtUses.stores.end());
        uses.dbRefs.append(edtUses.dbRefs.begin(), edtUses.dbRefs.end());
      }
    } else {
      uses.other.push_back(user);
    }
  }

  return uses;
}

DbRewriter::UseSet DbRewriter::collectUsesInEdt(Value source, EdtOp edt) {
  UseSet uses;
  SmallPtrSet<Value, 8> visited;

  edt.walk([&](Operation *op) {
    if (auto load = dyn_cast<memref::LoadOp>(op)) {
      if (arts::getUnderlyingOperation(load.getMemref()) ==
          source.getDefiningOp())
        uses.loads.push_back(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
      if (arts::getUnderlyingOperation(store.getMemref()) ==
          source.getDefiningOp())
        uses.stores.push_back(store);
    }
  });

  return uses;
}

///===----------------------------------------------------------------------===///
// Transformation
///===----------------------------------------------------------------------===///

LogicalResult DbRewriter::transformLoad(memref::LoadOp load,
                                        const StructuralDiff &diff) {
  OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPoint(load);
  Location loc = load.getLoc();

  SmallVector<Value> oldIndices(load.getIndices().begin(),
                                load.getIndices().end());

  if (!diff.needsIndexTransform) {
    /// No index transformation - just update memref source
    /// This will be handled by replaceAllUsesWith on the ptr
    return success();
  }

  /// Transform indices based on structural difference
  /// TODO: Implement index transformation logic here
  //   auto [outerIdx, innerIdx] =
  //   indexTransformer_->transformIndices(oldIndices);
  SmallVector<Value> outerIdx(oldIndices.begin(),
                              oldIndices.begin() + diff.oldOuterRank);
  SmallVector<Value> innerIdx(oldIndices.begin() + diff.oldOuterRank,
                              oldIndices.end());

  /// Create db_ref for outer indexing (chunk selection)
  Type elementMemRefType = newAlloc_.getAllocatedElementType();
  auto dbRef = builder_.create<DbRefOp>(loc, elementMemRefType,
                                        newAlloc_.getPtr(), outerIdx);

  /// Create new load with inner indices (local access within chunk)
  auto newLoad = builder_.create<memref::LoadOp>(loc, dbRef, innerIdx);

  /// Replace uses and mark old load for removal
  load.replaceAllUsesWith(newLoad.getResult());
  opsToRemove_.insert(load.getOperation());

  ARTS_INFO("    - Transformed load: " << oldIndices.size() << " indices → ("
                                       << outerIdx.size() << " outer, "
                                       << innerIdx.size() << " inner)");

  return success();
}

LogicalResult DbRewriter::transformStore(memref::StoreOp store,
                                         const StructuralDiff &diff) {
  OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPoint(store);
  Location loc = store.getLoc();

  SmallVector<Value> oldIndices(store.getIndices().begin(),
                                store.getIndices().end());

  if (!diff.needsIndexTransform) {
    /// No transformation needed
    return success();
  }

  /// Transform indices
  /// TODO: Implement index transformation logic here
  //   auto [outerIdx, innerIdx] =
  //   indexTransformer_->transformIndices(oldIndices);
  SmallVector<Value> outerIdx(oldIndices.begin(),
                              oldIndices.begin() + diff.oldOuterRank);
  SmallVector<Value> innerIdx(oldIndices.begin() + diff.oldOuterRank,
                              oldIndices.end());

  /// Create db_ref for outer indexing
  Type elementMemRefType = newAlloc_.getAllocatedElementType();
  auto dbRef = builder_.create<DbRefOp>(loc, elementMemRefType,
                                        newAlloc_.getPtr(), outerIdx);

  /// Create new store with inner indices
  Value valueToStore = store.getValue();
  builder_.create<memref::StoreOp>(loc, valueToStore, dbRef, innerIdx);

  /// Mark old store for removal
  opsToRemove_.insert(store.getOperation());

  ARTS_INFO("    - Transformed store: " << oldIndices.size() << " indices → ("
                                        << outerIdx.size() << " outer, "
                                        << innerIdx.size() << " inner)");

  return success();
}

LogicalResult DbRewriter::transformDbRef(DbRefOp dbRef,
                                         const StructuralDiff &diff) {
  /// DbRef operations will be handled by transforming their users
  /// (loads/stores) The dbRef itself will be replaced when we process its uses
  return success();
}

LogicalResult DbRewriter::transformDbAcquire(DbAcquireOp acquire,
                                             const StructuralDiff &diff) {
  /// Acquire operations may need their indices updated
  /// For now, we let replaceAllUsesWith handle the simple case
  /// TODO: Handle chunk-based acquires with index transformation
  return success();
}

LogicalResult DbRewriter::transformBlockArgument(BlockArgument arg,
                                                 Value newSource,
                                                 const StructuralDiff &diff) {
  /// Block arguments are handled through recursive use collection
  return success();
}

///===----------------------------------------------------------------------===///
// Helper Function Implementations
///===----------------------------------------------------------------------===///

/// Replace uses of old DbAllocOp with new promoted DbAllocOp
void DbRewriter::updateDbAllocAfterPromotion(
    DbAllocOp oldAlloc, DbAllocOp newAlloc, size_t pushCount,
    SetVector<Operation *> &opsToRemove) {
  /// Replace old alloc GUID uses
  oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());

  /// Update memref loads/stores that use the old pointer
  /// After promotion, multi-dimensional accesses need to be split
  Value oldPtr = oldAlloc.getPtr();
  Value newPtr = newAlloc.getPtr();
  /// Use the allocated element type from the new alloc
  Type elementMemRefType = newAlloc.getAllocatedElementType().getElementType();

  updateDbRefOpsAfterPromotion(oldPtr, elementMemRefType, opsToRemove, newPtr);

  /// Replace old alloc pointer uses
  oldPtr.replaceAllUsesWith(newPtr);
}

/// Update DbRefOp operations after datablock promotion
void DbRewriter::updateDbRefOpsAfterPromotion(
    Value sourcePtr, Type elementMemRefType,
    SetVector<Operation *> &opsToRemove, Value newSourcePtr) {
  SetVector<DbRefOp> refsToRewrite;

  for (Operation *user : sourcePtr.getUsers()) {
    if (auto refOp = dyn_cast<DbRefOp>(user)) {
      if (refOp.getSource() == sourcePtr)
        refsToRewrite.insert(refOp);
    }
  }

  /// Rewrite db_ref operations to match the new pointer rank hierarchy and
  /// preserve indexing semantics by forwarding dropped indices (suffix)
  /// into consumer load/store indices.
  for (auto refOp : refsToRewrite) {
    ARTS_DEBUG(" Rewriting db_ref " << refOp << " after promotion");
    ValueRange indices = refOp.getIndices();

    OpBuilder b(refOp);
    auto underlyingDb = arts::getUnderlyingDb(newSourcePtr);
    auto [prefix, suffix] =
        splitDbIndices(underlyingDb, indices, b, refOp.getLoc());

    /// Build DbRefOp with prefix indices.
    auto prefixRef = b.create<DbRefOp>(refOp.getLoc(), elementMemRefType,
                                       newSourcePtr, prefix);

    /// Forward any dropped indices into consumer load/store ops by
    /// prepending them to the existing index list.
    Value oldResult = refOp.getResult();
    SmallVector<Operation *> users(oldResult.getUsers().begin(),
                                   oldResult.getUsers().end());
    for (Operation *user : users) {
      if (auto ld = dyn_cast<memref::LoadOp>(user)) {
        SmallVector<Value> newIndices(suffix.begin(), suffix.end());
        ld.getMemrefMutable().assign(prefixRef);
        ld.getIndicesMutable().assign(newIndices);
      } else if (auto st = dyn_cast<memref::StoreOp>(user)) {
        SmallVector<Value> newIndices(suffix.begin(), suffix.end());
        st.getMemrefMutable().assign(prefixRef);
        st.getIndicesMutable().assign(newIndices);
      } else {
        user->replaceUsesOfWith(oldResult, prefixRef);
      }
    }

    refOp.erase();
  }
}

/// Update DbAcquireOp indices, offsets, and sizes after dimension promotion
void DbRewriter::updateDbAcquireAfterPromotion(
    DbAcquireOp acqOp, DbAllocOp newAlloc, size_t pushCount, OpBuilder &b,
    Location loc, SetVector<Operation *> &opsToRemove) {
  ValueRange oldIndices = acqOp.getIndices();
  ValueRange oldOffsets = acqOp.getOffsets();
  ValueRange oldSizes = acqOp.getSizes();

  SmallVector<Value> newIndices, newOffsets, newSizes;

  /// Move promoted indices to offsets
  if (!oldIndices.empty()) {
    newIndices.assign(oldIndices.begin() + pushCount, oldIndices.end());
    newOffsets.assign(oldIndices.begin(), oldIndices.begin() + pushCount);
    for (size_t i = 0; i < pushCount; ++i)
      newSizes.push_back(b.create<arith::ConstantIndexOp>(loc, 1));
  }

  /// Append existing offsets/sizes for non-promoted dimensions
  newOffsets.append(oldOffsets.begin() + pushCount, oldOffsets.end());
  newSizes.append(oldSizes.begin() + pushCount, oldSizes.end());

  /// Update the acquire operation
  acqOp.getIndicesMutable().assign(newIndices);
  acqOp.getOffsetsMutable().assign(newOffsets);
  acqOp.getSizesMutable().assign(newSizes);
  acqOp.getSourceGuidMutable().assign(newAlloc.getGuid());
  acqOp.getSourcePtrMutable().assign(newAlloc.getPtr());

  ARTS_DEBUG(" - Updated acquire " << acqOp << " after promotion");

  Type elementMemRefType = newAlloc.getAllocatedElementType().getElementType();
  auto [edt, blockArg] = arts::getEdtBlockArgumentForAcquire(acqOp);
  updateDbRefOpsAfterPromotion(blockArg, elementMemRefType, opsToRemove,
                               blockArg);
}

////===----------------------------------------------------------------------===////
/// Helper: Update DbAcquireOp for chunk-based partitioning
///
/// Creates a new DbAcquireOp with chunk indices and appropriate element sizes.
/// The new acquire will index into the chunk grid rather than acquiring the
/// entire array.
////===----------------------------------------------------------------------===////
static DbAcquireOp
updateAcquireForChunking(DbAcquireOp oldAcq, DbAllocOp newAlloc,
                         ArrayRef<Value> chunkIndices, Value chunkSizeVal,
                         OpBuilder &builder,
                         SetVector<Operation *> &opsToRemove) {

  // Use insertion guard to ensure proper scoping
  OpBuilder::InsertionGuard guard(builder);

  Location loc = oldAcq.getLoc();

  // Build element sizes - one chunk per dimension
  SmallVector<Value> elementSizes;
  size_t numDims = chunkIndices.size();
  for (size_t i = 0; i < numDims; ++i) {
    elementSizes.push_back(chunkSizeVal);
  }

  // Create new acquire with chunk indices
  // Note: offsets and sizes are empty - we're acquiring entire chunks
  builder.setInsertionPoint(oldAcq);

  // Convert ArrayRef to SmallVector for builder
  SmallVector<Value> indicesVec(chunkIndices.begin(), chunkIndices.end());
  SmallVector<Value> emptyOffsets;
  SmallVector<Value> emptySizes;

  DbAcquireOp newAcq = builder.create<DbAcquireOp>(
      loc, oldAcq.getMode(), newAlloc.getGuid(), newAlloc.getPtr(),
      indicesVec,   // indices: chunk coordinates
      emptyOffsets, // offsets: empty (acquiring whole chunks)
      emptySizes);  // sizes: empty (using elementSizes from alloc)

  // Update the EDT to use the new acquire's results
  oldAcq.getGuid().replaceAllUsesWith(newAcq.getGuid());
  oldAcq.getPtr().replaceAllUsesWith(newAcq.getPtr());

  // Mark old acquire for removal
  opsToRemove.insert(oldAcq.getOperation());

  return newAcq;
}

////===----------------------------------------------------------------------===////
/// Helper: Update load/store operations for chunk-based indexing
///
/// Transforms all memref.load, memref.store, and DbRefOp operations that use
/// the acquired memref to use local indices within the chunk instead of global
/// indices.
///
/// For each access with global index %i:
///   - Chunk offset: %offset = (%i / chunkSize) * chunkSize
///   - Local index: %local = %i - %offset
////===----------------------------------------------------------------------===////
static void updateLoadStoreForChunking(DbAcquireOp oldAcq, DbAcquireOp newAcq,
                                       Value chunkSizeVal, OpBuilder &builder,
                                       SetVector<Operation *> &opsToRemove) {

  // Use insertion guard to ensure proper scoping
  OpBuilder::InsertionGuard guard(builder);

  // Get the EDT and block argument for the old acquire
  auto [edt, oldBlockArg] = arts::getEdtBlockArgumentForAcquire(oldAcq);
  if (!edt || !oldBlockArg) {
    return;
  }

  // Get the new block argument (should be same position)
  auto [newEdt, newBlockArg] = arts::getEdtBlockArgumentForAcquire(newAcq);
  if (!newBlockArg) {
    return;
  }

  Location loc = oldAcq.getLoc();

  // Find all operations that use the old block argument
  SmallVector<Operation *> usersToUpdate;
  for (Operation *user : oldBlockArg.getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp, DbRefOp>(user)) {
      usersToUpdate.push_back(user);
    }
  }

  // Update each operation
  for (Operation *op : usersToUpdate) {
    if (auto dbRef = dyn_cast<DbRefOp>(op)) {
      // DbRefOp: db_ref %blockArg[%i, %j] -> memref<...>
      // After chunking: db_ref %blockArg[0, 0, ...] -> memref<16x16x...>
      // The chunk index is already in the acquire, so DbRefOp uses all zeros

      builder.setInsertionPoint(dbRef);
      SmallVector<Value> zeroIndices;
      Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);

      // Get the dimensionality from the old indices
      size_t numDims = dbRef.getIndices().size();
      for (size_t i = 0; i < numDims; ++i) {
        zeroIndices.push_back(zero);
      }

      // Update DbRefOp to use new block arg and zero indices
      dbRef.getSourceMutable().assign(newBlockArg);
      dbRef.getIndicesMutable().assign(zeroIndices);

      // Now we need to update the loads/stores that use this DbRefOp result
      Value dbRefResult = dbRef.getResult();
      SmallVector<Operation *> dbRefUsers;
      for (Operation *user : dbRefResult.getUsers()) {
        if (isa<memref::LoadOp, memref::StoreOp>(user)) {
          dbRefUsers.push_back(user);
        }
      }

      // For each load/store using the DbRefOp result
      for (Operation *loadStoreOp : dbRefUsers) {
        SmallVector<Value> origIndices;

        if (auto ld = dyn_cast<memref::LoadOp>(loadStoreOp)) {
          origIndices.assign(ld.getIndices().begin(), ld.getIndices().end());
        } else if (auto st = dyn_cast<memref::StoreOp>(loadStoreOp)) {
          origIndices.assign(st.getIndices().begin(), st.getIndices().end());
        } else {
          continue;
        }

        // Compute local indices: local = idx - (idx / chunkSize) * chunkSize
        builder.setInsertionPoint(loadStoreOp);
        SmallVector<Value> localIndices;

        for (Value idx : origIndices) {
          Value chunkIdx =
              builder.create<arith::DivUIOp>(loc, idx, chunkSizeVal);
          Value offset =
              builder.create<arith::MulIOp>(loc, chunkIdx, chunkSizeVal);
          Value localIdx = builder.create<arith::SubIOp>(loc, idx, offset);
          localIndices.push_back(localIdx);
        }

        // Update the operation with local indices
        if (auto ld = dyn_cast<memref::LoadOp>(loadStoreOp)) {
          ld.getIndicesMutable().assign(localIndices);
        } else if (auto st = dyn_cast<memref::StoreOp>(loadStoreOp)) {
          st.getIndicesMutable().assign(localIndices);
        }
      }
    }
  }
}

///===----------------------------------------------------------------------===///
// Cleanup and verification
///===----------------------------------------------------------------------===///

void DbRewriter::cleanup() {
  ModuleOp module = oldAlloc_->getParentOfType<ModuleOp>();
  OpRemovalManager removalMgr;
  for (Operation *op : opsToRemove_)
    removalMgr.markForRemoval(op);
  stats_.operationsRemoved = opsToRemove_.size();
  removalMgr.removeAllMarked(module, config_.recursiveRemoval);
}

LogicalResult DbRewriter::verify() {
  /// Check that old allocation has no remaining uses (except what we tracked)
  for (auto &use : oldAlloc_.getGuid().getUses()) {
    if (!opsToRemove_.contains(use.getOwner())) {
      return oldAlloc_.emitError("Old allocation GUID still has uses after "
                                 "rewriting");
    }
  }

  for (auto &use : oldAlloc_.getPtr().getUses()) {
    if (!opsToRemove_.contains(use.getOwner())) {
      return oldAlloc_.emitError("Old allocation ptr still has uses after "
                                 "rewriting");
    }
  }

  return success();
}

bool mlir::arts::rewriteDbUserOperation(
    Operation *op, Type elementMemRefType, Value basePtr, Operation *dbOp,
    uint64_t initialIndex, ArrayRef<Value> chunkOffsets, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    Location loc = dbRef.getLoc();
    builder.setInsertionPoint(dbRef);
    SmallVector<Value> zeroIndices;
    Value zero = builder.create<arith::ConstantIndexOp>(loc, 0);
    for (size_t i = 0, e = dbRef.getIndices().size(); i < e; ++i)
      zeroIndices.push_back(zero);
    dbRef.getSourceMutable().assign(basePtr);
    dbRef.getIndicesMutable().assign(zeroIndices);

    Value refResult = dbRef.getResult();
    SmallVector<Operation *> refUsers;
    for (Operation *user : refResult.getUsers()) {
      if (isa<memref::LoadOp, memref::StoreOp>(user))
        refUsers.push_back(user);
    }

    for (Operation *user : refUsers) {
      builder.setInsertionPoint(user);
      auto adjustWithOffsets = [&](OperandRange idxRange) {
        SmallVector<Value> remapped(idxRange.begin(), idxRange.end());
        size_t limit = std::min(remapped.size(), chunkOffsets.size());
        for (size_t i = 0; i < limit; ++i) {
          if (!chunkOffsets[i])
            continue;
          remapped[i] = builder.create<arith::SubIOp>(
              user->getLoc(), remapped[i], chunkOffsets[i]);
        }
        return remapped;
      };

      if (auto load = dyn_cast<memref::LoadOp>(user)) {
        SmallVector<Value> localIdx = adjustWithOffsets(load.getIndices());
        load.getIndicesMutable().assign(localIdx);
      } else if (auto store = dyn_cast<memref::StoreOp>(user)) {
        SmallVector<Value> localIdx = adjustWithOffsets(store.getIndices());
        store.getIndicesMutable().assign(localIdx);
      }
    }

    return true;
  }

  auto adjustIndices = [&](mlir::OperandRange rawIndices) {
    SmallVector<Value> indices;
    if (rawIndices.size() > initialIndex) {
      indices.append(rawIndices.begin() + initialIndex, rawIndices.end());
    }
    if (!chunkOffsets.empty()) {
      size_t limit = std::min(indices.size(), chunkOffsets.size());
      for (size_t i = 0; i < limit; ++i) {
        if (!chunkOffsets[i])
          continue;
        indices[i] = builder.create<arith::SubIOp>(op->getLoc(), indices[i],
                                                   chunkOffsets[i]);
      }
    }
    return indices;
  };

  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    Location loc = load.getLoc();
    builder.setInsertionPoint(load);
    SmallVector<Value> indices = adjustIndices(load.getIndices());
    auto [outerIdx, innerIdx] = splitDbIndices(dbOp, indices, builder, loc);
    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, outerIdx);
    auto newLoad = builder.create<memref::LoadOp>(loc, dbRef, innerIdx);
    load.replaceAllUsesWith(newLoad.getResult());
    opsToRemove.insert(load);
    return true;
  }

  if (auto store = dyn_cast<memref::StoreOp>(op)) {
    Location loc = store.getLoc();
    builder.setInsertionPoint(store);
    SmallVector<Value> indices = adjustIndices(store.getIndices());
    auto [outerIdx, innerIdx] = splitDbIndices(dbOp, indices, builder, loc);
    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, outerIdx);
    builder.create<memref::StoreOp>(loc, store.getValue(), dbRef, innerIdx);
    opsToRemove.insert(store);
    return true;
  }

  if (auto m2p = dyn_cast<polygeist::Memref2PointerOp>(op)) {
    Location loc = m2p.getLoc();
    builder.setInsertionPoint(m2p);
    SmallVector<Value> indicesConst0(
        {builder.create<arith::ConstantIndexOp>(loc, 0)});
    auto dbRef =
        builder.create<DbRefOp>(loc, elementMemRefType, basePtr, indicesConst0);
    auto newM2p = builder.create<polygeist::Memref2PointerOp>(
        loc, m2p.getResult().getType(), dbRef.getResult());
    m2p.getResult().replaceAllUsesWith(newM2p.getResult());
    opsToRemove.insert(m2p);
    return true;
  }

  return false;
}

///===----------------------------------------------------------------------===///
/// Index Splitting Utilities for Datablocks
///===----------------------------------------------------------------------===///

/// Splits datablock indices into outer and inner indices based on the
/// datablock's structure. For a memref<?xmemref<?xi32>> with indices [i, j, k]:
/// - Outer indices [i]: select which element memref
/// - Inner indices [j, k]: index into the element memref
std::pair<SmallVector<Value>, SmallVector<Value>>
mlir::arts::splitDbIndices(Operation *dbOp, ValueRange indices,
                           OpBuilder &builder, Location loc) {
  SmallVector<Value> outerIndices, innerIndices;

  /// Get datablock properties
  auto hasSingleSize = dbHasSingleSize(dbOp);
  auto outerRank = getSizesFromDb(dbOp).size();

  /// If the Db has single size, all the indices are inner indices
  if (hasSingleSize)
    innerIndices.assign(indices.begin(), indices.end());

  /// If the indices are less than or equal to the outer rank, all the indices
  /// are outer indices
  else if (indices.size() <= outerRank)
    outerIndices.assign(indices.begin(), indices.end());

  /// If the indices are greater than the outer rank, split the indices into
  /// outer and inner indices
  else {
    outerIndices.assign(indices.begin(), indices.begin() + outerRank);
    innerIndices.assign(indices.begin() + outerRank, indices.end());
  }

  /// For all other cases, add a default index of 0
  if (outerIndices.empty())
    outerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (innerIndices.empty())
    innerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return std::make_pair(outerIndices, innerIndices);
}
