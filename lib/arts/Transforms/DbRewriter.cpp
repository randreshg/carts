///==========================================================================///
/// File: DbRewriter.cpp
///
/// Implementation of the DbRewriter class for automatic datablock use
/// transformation when allocation structures change.
///==========================================================================///

#include "arts/Transforms/DbRewriter.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

ARTS_DEBUG_SETUP(db_rewriter);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// IndexMapping Implementation
///===----------------------------------------------------------------------===///

bool mlir::arts::IndexMapping::isIdentity() const {
  /// Identity: outer->outer (same position), inner->inner (same position)
  for (size_t i = 0; i < newOuterIndices.size(); ++i) {
    if (newOuterIndices[i].source != Source::OuterIndex ||
        newOuterIndices[i].sourcePosition != i)
      return false;
  }
  for (size_t i = 0; i < newInnerIndices.size(); ++i) {
    if (newInnerIndices[i].source != Source::InnerIndex ||
        newInnerIndices[i].sourcePosition != i)
      return false;
  }
  return true;
}

bool mlir::arts::IndexMapping::isCoarseToFine() const {
  /// Coarse->Fine: new outer comes from old inner, new inner is constant 0
  if (newOuterIndices.empty())
    return false;
  for (const auto &entry : newOuterIndices) {
    if (entry.source != Source::InnerIndex)
      return false;
  }
  for (const auto &entry : newInnerIndices) {
    if (entry.source != Source::ConstantZero)
      return false;
  }
  return true;
}

bool mlir::arts::IndexMapping::isFineToCoarse() const {
  /// Fine->Coarse: new outer is constant 0, new inner comes from old outer
  for (const auto &entry : newOuterIndices) {
    if (entry.source != Source::ConstantZero)
      return false;
  }
  if (newInnerIndices.empty())
    return false;
  for (const auto &entry : newInnerIndices) {
    if (entry.source != Source::OuterIndex)
      return false;
  }
  return true;
}

///===----------------------------------------------------------------------===///
/// DbRewriter Constructor
///===----------------------------------------------------------------------===///

DbRewriter::DbRewriter(MLIRContext *context, Config config)
    : config_(config), builder_(context) {}

///===----------------------------------------------------------------------===///
/// Core Index Mapping Functions
///===----------------------------------------------------------------------===///

IndexMapping DbRewriter::computeIndexMapping(DbAllocOp oldAlloc,
                                             DbAllocOp newAlloc) {
  IndexMapping mapping;

  ValueRange oldSizes = oldAlloc.getSizes();
  ValueRange oldElemSizes = oldAlloc.getElementSizes();
  ValueRange newSizes = newAlloc.getSizes();
  ValueRange newElemSizes = newAlloc.getElementSizes();

  size_t newOuterRank = newSizes.size();
  size_t newInnerRank = newElemSizes.size();

  /// Check if old structure is coarse-grained (all sizes are constant 1)
  bool oldSizesUnit = llvm::all_of(oldSizes, [](Value v) {
    int64_t val;
    return arts::getConstantIndex(v, val) && val == 1;
  });

  /// Check if new structure is fine-grained (all elementSizes are constant 1)
  bool newElemSizesUnit = llvm::all_of(newElemSizes, [](Value v) {
    int64_t val;
    return arts::getConstantIndex(v, val) && val == 1;
  });

  /// Check if old structure is fine-grained
  bool oldElemSizesUnit = llvm::all_of(oldElemSizes, [](Value v) {
    int64_t val;
    return arts::getConstantIndex(v, val) && val == 1;
  });

  /// Check if new structure is coarse-grained
  bool newSizesUnit = llvm::all_of(newSizes, [](Value v) {
    int64_t val;
    return arts::getConstantIndex(v, val) && val == 1;
  });

  /// Detect coarse-to-fine transformation
  /// OLD: sizes=[1,...], elementSizes=[N,...]
  /// NEW: sizes=[N,...], elementSizes=[1,...]
  bool isCoarseToFine = oldSizesUnit && newElemSizesUnit &&
                        !oldElemSizes.empty() && !newSizes.empty();

  /// Detect fine-to-coarse transformation
  /// OLD: sizes=[N,...], elementSizes=[1,...]
  /// NEW: sizes=[1,...], elementSizes=[N,...]
  bool isFineToCoarse = newSizesUnit && oldElemSizesUnit && !oldSizes.empty() &&
                        !newElemSizes.empty();

  if (isCoarseToFine) {
    ARTS_DEBUG("Detected coarse-to-fine transformation");
    /// Coarse->Fine: old inner indices -> new outer, new inner = constant 0
    /// db_ref[0] + load[i,j] -> db_ref[i,j] + load[0]
    for (size_t i = 0; i < newOuterRank; ++i) {
      mapping.newOuterIndices.push_back(
          {IndexMapping::Source::InnerIndex, static_cast<unsigned>(i)});
    }
    for (size_t i = 0; i < newInnerRank; ++i) {
      mapping.newInnerIndices.push_back(
          {IndexMapping::Source::ConstantZero, 0});
    }
  } else if (isFineToCoarse) {
    ARTS_DEBUG("Detected fine-to-coarse transformation");
    /// Fine->Coarse: new outer = constant 0, old outer indices -> new inner
    /// db_ref[i,j] + load[0] -> db_ref[0] + load[i,j]
    for (size_t i = 0; i < newOuterRank; ++i) {
      mapping.newOuterIndices.push_back(
          {IndexMapping::Source::ConstantZero, 0});
    }
    for (size_t i = 0; i < newInnerRank; ++i) {
      mapping.newInnerIndices.push_back(
          {IndexMapping::Source::OuterIndex, static_cast<unsigned>(i)});
    }
  } else {
    ARTS_DEBUG("Using identity/positional mapping");
    size_t oldOuterRank = oldSizes.size();
    size_t minOuterRank = std::min(oldOuterRank, newOuterRank);
    for (size_t i = 0; i < minOuterRank; ++i) {
      mapping.newOuterIndices.push_back(
          {IndexMapping::Source::OuterIndex, static_cast<unsigned>(i)});
    }

    /// Pad remaining outer with zeros if needed
    for (size_t i = minOuterRank; i < newOuterRank; ++i) {
      mapping.newOuterIndices.push_back(
          {IndexMapping::Source::ConstantZero, 0});
    }

    /// Map inner->inner up to min rank
    size_t oldInnerRank = oldElemSizes.size();
    size_t minInnerRank = std::min(oldInnerRank, newInnerRank);
    for (size_t i = 0; i < minInnerRank; ++i) {
      mapping.newInnerIndices.push_back(
          {IndexMapping::Source::InnerIndex, static_cast<unsigned>(i)});
    }

    /// Pad remaining inner with zeros if needed
    for (size_t i = minInnerRank; i < newInnerRank; ++i) {
      mapping.newInnerIndices.push_back(
          {IndexMapping::Source::ConstantZero, 0});
    }
  }

  return mapping;
}

std::pair<SmallVector<Value>, SmallVector<Value>>
DbRewriter::transformIndices(ArrayRef<Value> oldOuterIndices,
                             ArrayRef<Value> oldInnerIndices,
                             OpBuilder &builder, Location loc) {
  SmallVector<Value> newOuter, newInner;

  auto resolveEntry = [&](const IndexMapping::Entry &entry) -> Value {
    switch (entry.source) {
    case IndexMapping::Source::OuterIndex:
      if (entry.sourcePosition < oldOuterIndices.size())
        return oldOuterIndices[entry.sourcePosition];
      /// Fallback to constant 0 if index out of range
      return builder.create<arith::ConstantIndexOp>(loc, 0);

    case IndexMapping::Source::InnerIndex:
      if (entry.sourcePosition < oldInnerIndices.size())
        return oldInnerIndices[entry.sourcePosition];
      /// Fallback to constant 0 if index out of range
      return builder.create<arith::ConstantIndexOp>(loc, 0);

    case IndexMapping::Source::ConstantZero:
      return builder.create<arith::ConstantIndexOp>(loc, 0);
    }
    llvm_unreachable("Invalid source type");
  };

  for (const auto &entry : mapping_.newOuterIndices)
    newOuter.push_back(resolveEntry(entry));
  for (const auto &entry : mapping_.newInnerIndices)
    newInner.push_back(resolveEntry(entry));

  /// Ensure non-empty (fallback to constant 0)
  if (newOuter.empty())
    newOuter.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
  if (newInner.empty())
    newInner.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));

  return {newOuter, newInner};
}

///===----------------------------------------------------------------------===///
/// Main Entry Point
///===----------------------------------------------------------------------===///

LogicalResult DbRewriter::rewriteAllUses(DbAllocOp oldAlloc,
                                         DbAllocOp newAlloc) {
  oldAlloc_ = oldAlloc;
  newAlloc_ = newAlloc;

  if (!oldAlloc_ || !oldAlloc_.getOperation() || !newAlloc_ ||
      !newAlloc_.getOperation()) {
    ARTS_DEBUG("Invalid allocation ops");
    return failure();
  }

  /// Step 1: Compute the index mapping
  mapping_ = computeIndexMapping(oldAlloc, newAlloc);

  ARTS_DEBUG_REGION(
      ARTS_DBGS() << "Transformation analysis:\n"
                  << "  - Old alloc: sizes=" << oldAlloc.getSizes().size()
                  << ", elemSizes=" << oldAlloc.getElementSizes().size() << "\n"
                  << "  - New alloc: sizes=" << newAlloc.getSizes().size()
                  << ", elemSizes=" << newAlloc.getElementSizes().size() << "\n"
                  << "  - Mapping: " << mapping_.newOuterIndices.size()
                  << " outer, " << mapping_.newInnerIndices.size() << " inner\n"
                  << "  - isCoarseToFine: " << mapping_.isCoarseToFine() << "\n"
                  << "  - isFineToCoarse: " << mapping_.isFineToCoarse() << "\n"
                  << "  - isIdentity: " << mapping_.isIdentity() << "\n";);

  /// Step 2: Collect all uses recursively
  SmallPtrSet<Value, 8> visited;
  UseSet uses = collectUsesRecursive(oldAlloc.getPtr(), visited);

  ARTS_DEBUG_REGION(ARTS_DBGS()
                        << "Found " << uses.size() << " uses\n"
                        << "  - Loads: " << uses.loads.size() << "\n"
                        << "  - Stores: " << uses.stores.size() << "\n"
                        << "  - DbRefs: " << uses.dbRefs.size() << "\n"
                        << "  - Acquires: " << uses.acquires.size() << "\n";);

  if (uses.empty()) {
    ARTS_DEBUG("No uses found, replacing directly");
    oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
    oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());
    if (config_.removeDeadOps) {
      opsToRemove_.insert(oldAlloc.getOperation());
    }
    return success();
  }

  /// Step 3: Transform each use type
  ARTS_DEBUG("Transforming uses...");

  for (auto load : uses.loads) {
    if (failed(transformLoad(load)))
      return failure();
    stats_.loadsTransformed++;
  }

  for (auto store : uses.stores) {
    if (failed(transformStore(store)))
      return failure();
    stats_.storesTransformed++;
  }

  for (auto dbRef : uses.dbRefs) {
    if (failed(transformDbRef(dbRef)))
      return failure();
    stats_.dbRefsUpdated++;
  }

  for (auto acquire : uses.acquires) {
    if (failed(transformDbAcquire(acquire)))
      return failure();
    stats_.acquiresUpdated++;
  }

  ARTS_DEBUG_REGION(ARTS_DBGS()
                        << "Transformed: " << stats_.loadsTransformed
                        << " loads, " << stats_.storesTransformed << " stores, "
                        << stats_.dbRefsUpdated << " dbRefs, "
                        << stats_.acquiresUpdated << " acquires\n";);

  /// Step 4: Replace GUID and pointer uses
  oldAlloc.getGuid().replaceAllUsesWith(newAlloc.getGuid());
  oldAlloc.getPtr().replaceAllUsesWith(newAlloc.getPtr());

  /// Step 5: Verify (if enabled)
  if (config_.preserveSemantics) {
    if (failed(verify())) {
      ARTS_DEBUG("Verification failed!");
      return failure();
    }
    ARTS_DEBUG("Verification passed");
  }

  /// Step 6: Cleanup
  if (config_.removeDeadOps) {
    opsToRemove_.insert(oldAlloc.getOperation());
    cleanup();
    ARTS_DEBUG("Removed " << stats_.operationsRemoved << " operations");
  }

  ARTS_DEBUG("Transformation complete!");
  return success();
}

///===----------------------------------------------------------------------===///
/// Use Collection
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
/// Transformation (Simplified - uses IndexMapping)
///===----------------------------------------------------------------------===///

LogicalResult DbRewriter::transformLoad(memref::LoadOp load) {
  OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPoint(load);
  Location loc = load.getLoc();

  /// Get source db_ref if present
  Value memref = load.getMemref();
  DbRefOp sourceDbRef = memref.getDefiningOp<DbRefOp>();

  ARTS_DEBUG_REGION(ARTS_DBGS()
                        << "Transforming load with"
                        << " sourceDbRef=" << (sourceDbRef ? "yes" : "no")
                        << ", memref isBlockArg=" << memref.isa<BlockArgument>()
                        << "\n";);

  /// Collect old indices
  SmallVector<Value> oldOuterIndices, oldInnerIndices;
  if (sourceDbRef) {
    oldOuterIndices.append(sourceDbRef.getIndices().begin(),
                           sourceDbRef.getIndices().end());
  }
  oldInnerIndices.append(load.getIndices().begin(), load.getIndices().end());

  /// Apply the transformation - SINGLE UNIFIED PATH
  auto [newOuterIndices, newInnerIndices] =
      transformIndices(oldOuterIndices, oldInnerIndices, builder_, loc);

  /// Determine the base pointer to use:
  /// - If there was a source db_ref, use its source
  /// - Otherwise, use the memref directly
  Value basePtr;
  if (sourceDbRef) {
    basePtr = sourceDbRef.getSource();
  } else {
    basePtr = memref;
    ARTS_DEBUG("Using memref as basePtr (block arg): " << memref);
  }

  /// Create new db_ref and load
  /// Note: getAllocatedElementType() returns MemRefType, but db_ref needs its
  /// element type
  Type elementMemRefType = newAlloc_.getAllocatedElementType().getElementType();
  auto newDbRef = builder_.create<DbRefOp>(loc, elementMemRefType, basePtr,
                                           newOuterIndices);
  auto newLoad =
      builder_.create<memref::LoadOp>(loc, newDbRef, newInnerIndices);

  /// Replace and cleanup
  load.replaceAllUsesWith(newLoad.getResult());
  opsToRemove_.insert(load.getOperation());
  if (sourceDbRef && sourceDbRef.getResult().hasOneUse())
    opsToRemove_.insert(sourceDbRef.getOperation());

  ARTS_DEBUG("Transformed load: outer=" << newOuterIndices.size() << ", inner="
                                        << newInnerIndices.size());

  return success();
}

LogicalResult DbRewriter::transformStore(memref::StoreOp store) {
  OpBuilder::InsertionGuard guard(builder_);
  builder_.setInsertionPoint(store);
  Location loc = store.getLoc();

  /// Get source db_ref if present
  Value memref = store.getMemref();
  DbRefOp sourceDbRef = memref.getDefiningOp<DbRefOp>();

  ARTS_DEBUG_REGION(ARTS_DBGS()
                        << "Transforming store with"
                        << " sourceDbRef=" << (sourceDbRef ? "yes" : "no")
                        << ", memref isBlockArg=" << memref.isa<BlockArgument>()
                        << "\n";);

  /// Collect old indices
  SmallVector<Value> oldOuterIndices, oldInnerIndices;
  if (sourceDbRef) {
    oldOuterIndices.append(sourceDbRef.getIndices().begin(),
                           sourceDbRef.getIndices().end());
  }
  oldInnerIndices.append(store.getIndices().begin(), store.getIndices().end());

  /// Apply the transformation
  auto [newOuterIndices, newInnerIndices] =
      transformIndices(oldOuterIndices, oldInnerIndices, builder_, loc);

  /// Determine the base pointer to use:
  /// - If there was a source db_ref, use its source
  /// - Otherwise, use the memref directly
  Value basePtr;
  if (sourceDbRef) {
    basePtr = sourceDbRef.getSource();
  } else {
    basePtr = memref;
    ARTS_DEBUG("Using memref as basePtr (block arg): " << memref);
  }

  /// Create new db_ref and store

  Type elementMemRefType = newAlloc_.getAllocatedElementType().getElementType();
  auto newDbRef = builder_.create<DbRefOp>(loc, elementMemRefType, basePtr,
                                           newOuterIndices);
  Value valueToStore = store.getValue();
  builder_.create<memref::StoreOp>(loc, valueToStore, newDbRef,
                                   newInnerIndices);

  /// Cleanup
  opsToRemove_.insert(store.getOperation());
  if (sourceDbRef && sourceDbRef.getResult().hasOneUse())
    opsToRemove_.insert(sourceDbRef.getOperation());

  ARTS_DEBUG("Transformed store: outer=" << newOuterIndices.size() << ", inner="
                                         << newInnerIndices.size());

  return success();
}

LogicalResult DbRewriter::transformDbRef(DbRefOp dbRef) {
  /// DbRef operations will be handled by transforming their users
  /// (loads/stores) The dbRef itself will be replaced when we process its uses
  return success();
}

LogicalResult DbRewriter::transformDbAcquire(DbAcquireOp acquire) {
  /// Update source pointers to point to new alloc
  acquire.getSourceGuidMutable().assign(newAlloc_.getGuid());
  acquire.getSourcePtrMutable().assign(newAlloc_.getPtr());
  return success();
}

///===----------------------------------------------------------------------===///
/// Cleanup and Verification
///===----------------------------------------------------------------------===///

void DbRewriter::cleanup() {
  /// Sort operations by dominance order (remove uses before defs)
  SmallVector<Operation *> sortedOps(opsToRemove_.begin(), opsToRemove_.end());

  /// Simple reverse iteration often works for typical use patterns
  for (auto it = sortedOps.rbegin(); it != sortedOps.rend(); ++it) {
    Operation *op = *it;
    if (op && op->use_empty()) {
      op->erase();
      stats_.operationsRemoved++;
    }
  }

  opsToRemove_.clear();
}

LogicalResult DbRewriter::verify() {
  /// Basic verification: ensure new alloc is valid
  if (!newAlloc_ || !newAlloc_.getOperation())
    return failure();

  /// Verify all loads/stores reference valid memrefs
  /// (This is a lightweight check - full verification would need more)
  return success();
}

void DbRewriter::Statistics::print(llvm::raw_ostream &os) const {
  os << "DbRewriter Statistics:\n";
  os << "  Loads transformed:  " << loadsTransformed << "\n";
  os << "  Stores transformed: " << storesTransformed << "\n";
  os << "  DbRefs updated:     " << dbRefsUpdated << "\n";
  os << "  Acquires updated:   " << acquiresUpdated << "\n";
  os << "  Operations removed: " << operationsRemoved << "\n";
}

///===----------------------------------------------------------------------===///
/// Shared Helper Functions
///===----------------------------------------------------------------------===///

bool mlir::arts::rewriteDbUserOperation(
    Operation *op, Type elementMemRefType, Value basePtr, Operation *dbOp,
    uint64_t initialIndex, ArrayRef<Value> chunkOffsets, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPoint(op);
  Location loc = op->getLoc();

  if (auto dbRef = dyn_cast<DbRefOp>(op)) {
    SmallVector<Value> outerIndices(dbRef.getIndices().begin(),
                                    dbRef.getIndices().end());
    if (!chunkOffsets.empty()) {
      for (size_t i = 0; i < std::min(outerIndices.size(), chunkOffsets.size());
           ++i)
        outerIndices[i] = builder.create<arith::SubIOp>(loc, outerIndices[i],
                                                        chunkOffsets[i]);
    }
    if (outerIndices.empty())
      outerIndices.push_back(
          builder.create<arith::ConstantIndexOp>(loc, initialIndex));

    auto newRef = builder.create<DbRefOp>(loc, dbRef.getResult().getType(),
                                          basePtr, outerIndices);
    dbRef.replaceAllUsesWith(newRef.getResult());
    opsToRemove.insert(dbRef.getOperation());
    return true;
  }

  /// Helper to check if all indices are constant zeros (fine-grained style)
  auto allIndicesAreZero = [](ArrayRef<Value> indices) {
    return llvm::all_of(indices, [](Value v) {
      int64_t cst;
      return arts::getConstantIndex(v, cst) && cst == 0;
    });
  };

  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    SmallVector<Value> oldInnerIndices(load.getIndices().begin(),
                                       load.getIndices().end());
    Value memref = load.getMemref();

    /// Use getUnderlyingDb to check if already in DB world
    /// Returns DbAcquireOp/DbAllocOp if in DB world, null if raw memref
    Operation *dbOp = arts::getUnderlyingDb(memref);

    /// Guard: skip if ALREADY in DB world AND fine-grained style
    /// Only skip if the memref traces back to a DB operation (already
    /// transformed) If dbOp is null, we're in raw memref world and need to
    /// transform
    if (dbOp && allIndicesAreZero(oldInnerIndices)) {
      return false;
    }

    /// Error check for unknown patterns (optional safety)
    if (!dbOp) {
      Operation *rawOp = arts::getUnderlyingOperation(memref);
      if (!rawOp ||
          (!isa<memref::AllocOp>(rawOp) && !isa<memref::AllocaOp>(rawOp))) {
        op->emitError("rewriteDbUserOperation: unknown memref origin for load");
        return false;
      }
    }

    /// COARSE-TO-FINE TRANSFORMATION:
    /// OLD: db_ref[outer] + load[inner]
    /// NEW: db_ref[outer ++ inner_adjusted] + load[zeros]
    ///
    /// The old inner indices become the new outer indices.
    /// New inner indices are all zeros (fine-grained: 1 element per datablock).
    SmallVector<Value> newOuterIndices;
    SmallVector<Value> newInnerIndices;

    /// Transform old inner indices to new outer indices
    /// Apply offset adjustment if chunk-based acquire
    for (size_t i = 0; i < oldInnerIndices.size(); ++i) {
      Value idx = oldInnerIndices[i];
      /// If we have chunk offsets, subtract them for local indexing
      if (i < chunkOffsets.size()) {
        idx = builder.create<arith::SubIOp>(loc, idx, chunkOffsets[i]);
      }
      newOuterIndices.push_back(idx);
    }

    /// Handle case when acquire indices already selected the element
    /// (initialIndex > 0 means N dimensions were consumed by acquire)
    if (initialIndex > 0 && newOuterIndices.size() >= initialIndex) {
      /// Remove the first N indices - they were handled by the acquire
      newOuterIndices.erase(newOuterIndices.begin(),
                            newOuterIndices.begin() + initialIndex);
    }

    /// If no outer indices remain, use constant 0
    if (newOuterIndices.empty()) {
      newOuterIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    }

    /// New inner indices are all zeros (fine-grained: 1 element per datablock)
    auto rank = elementMemRefType.cast<MemRefType>().getRank();
    for (int64_t i = 0; i < rank; ++i) {
      newInnerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    }

    auto dbRef = builder.create<DbRefOp>(loc, elementMemRefType, basePtr,
                                         newOuterIndices);
    auto newLoad = builder.create<memref::LoadOp>(loc, dbRef, newInnerIndices);
    load.replaceAllUsesWith(newLoad.getResult());
    opsToRemove.insert(load.getOperation());
    return true;
  }

  if (auto store = dyn_cast<memref::StoreOp>(op)) {
    SmallVector<Value> oldInnerIndices(store.getIndices().begin(),
                                       store.getIndices().end());
    Value memref = store.getMemref();

    /// Use getUnderlyingDb to check if already in DB world
    /// Returns DbAcquireOp/DbAllocOp if in DB world, null if raw memref
    Operation *dbOp = arts::getUnderlyingDb(memref);

    /// Guard: skip if ALREADY in DB world AND fine-grained style
    /// Only skip if the memref traces back to a DB operation (already
    /// transformed) If dbOp is null, we're in raw memref world and need to
    /// transform
    if (dbOp && allIndicesAreZero(oldInnerIndices)) {
      return false;
    }

    /// Error check for unknown patterns (optional safety)
    if (!dbOp) {
      Operation *rawOp = arts::getUnderlyingOperation(memref);
      if (!rawOp ||
          (!isa<memref::AllocOp>(rawOp) && !isa<memref::AllocaOp>(rawOp))) {
        op->emitError(
            "rewriteDbUserOperation: unknown memref origin for store");
        return false;
      }
    }

    /// COARSE-TO-FINE TRANSFORMATION:
    /// OLD: db_ref[outer] + store[inner]
    /// NEW: db_ref[outer ++ inner_adjusted] + store[zeros]
    ///
    /// The old inner indices become the new outer indices.
    /// New inner indices are all zeros (fine-grained: 1 element per datablock).
    SmallVector<Value> newOuterIndices, newInnerIndices;

    /// Transform old inner indices to new outer indices
    /// Apply offset adjustment if chunk-based acquire
    for (size_t i = 0; i < oldInnerIndices.size(); ++i) {
      Value idx = oldInnerIndices[i];
      /// If we have chunk offsets, subtract them for local indexing
      if (i < chunkOffsets.size()) {
        idx = builder.create<arith::SubIOp>(loc, idx, chunkOffsets[i]);
      }
      newOuterIndices.push_back(idx);
    }

    /// Handle case when acquire indices already selected the element
    /// (initialIndex > 0 means N dimensions were consumed by acquire)
    if (initialIndex > 0 && newOuterIndices.size() >= initialIndex) {
      /// Remove the first N indices - they were handled by the acquire
      newOuterIndices.erase(newOuterIndices.begin(),
                            newOuterIndices.begin() + initialIndex);
    }

    /// If no outer indices remain, use constant 0
    if (newOuterIndices.empty()) {
      newOuterIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    }

    /// New inner indices are all zeros (fine-grained: 1 element per datablock)
    auto rank = elementMemRefType.cast<MemRefType>().getRank();
    for (int64_t i = 0; i < rank; ++i) {
      newInnerIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
    }

    auto dbRef = builder.create<DbRefOp>(loc, elementMemRefType, basePtr,
                                         newOuterIndices);
    builder.create<memref::StoreOp>(loc, store.getValue(), dbRef,
                                    newInnerIndices);
    opsToRemove.insert(store.getOperation());
    return true;
  }

  return false;
}
