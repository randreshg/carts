///==========================================================================///
/// File: DbIndexerBase.cpp
///
/// Shared implementations for DbIndexerBase template methods.
///
/// This file contains the common operation-rewriting logic shared by
/// DbBlockIndexer and DbElementWiseIndexer. The template method pattern
/// delegates mode-specific behavior to virtual hooks:
/// - detectLinearizedStride(): how to detect linearized access
/// - handleSubIndexOp(): how to rewrite polygeist::SubIndexOp
/// - handleEmptyIndices(): how to handle ops with no indices
/// - localizeForDbRefUser(): how to localize indices in transformDbRefUsers
///
/// Before:
///   %ref = arts.db_ref %db[%c0]
///   %v = memref.load %ref[%i, %j]
///
/// After:
///   %ref = arts.db_ref %db[%block]
///   %v = memref.load %ref[%local_i, %local_j]
///==========================================================================///

#include "arts/transforms/db/DbIndexerBase.h"
#include "arts/codegen/Codegen.h"
#include "arts/utils/DbUtils.h"
#include "arts/utils/Debug.h"
#include "arts/utils/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "polygeist/Ops.h"

ARTS_DEBUG_SETUP(db_transforms);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Default virtual hook implementations
///===----------------------------------------------------------------------===///

std::pair<bool, Value> DbIndexerBase::detectLinearizedStride(ValueRange indices,
                                                             Type elementType,
                                                             OpBuilder &builder,
                                                             Location loc) {
  /// Default: check if single index accesses a multi-dimensional MemRefType
  if (indices.size() != 1)
    return {false, Value()};

  auto memrefType = dyn_cast<MemRefType>(elementType);
  if (!memrefType || memrefType.getRank() < 2)
    return {false, Value()};

  auto staticStride = DbUtils::getStaticStride(memrefType);
  if (!staticStride || *staticStride <= 1)
    return {false, Value()};

  Value stride = arts::createConstantIndex(builder, loc, *staticStride);
  return {true, stride};
}

bool DbIndexerBase::handleSubIndexOp(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  auto subindex = dyn_cast<polygeist::SubIndexOp>(op);
  if (!subindex)
    return false;

  Location loc = op->getLoc();
  auto zero = AC.createIndexConstant(0, loc);

  if (outerRank == 0) {
    /// Coarse mode: keep subindex semantics, just redirect the source.
    auto dbRef = AC.create<DbRefOp>(loc, elementType, dbPtr, ValueRange{zero});
    subindex.getSourceMutable().assign(dbRef.getResult());
    ARTS_DEBUG("  Redirected subindex source to db_ref (coarse mode)");
    return true;
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
  return true;
}

bool DbIndexerBase::handleEmptyIndices(
    Operation *op, Value dbPtr, Type elementType, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  /// Default: skip ops with empty indices
  return false;
}

LocalizedIndices DbIndexerBase::localizeForDbRefUser(ValueRange elementIndices,
                                                     Type newElementType,
                                                     OpBuilder &builder,
                                                     Location loc) {
  /// Default: detect linearized access then call localize/localizeLinearized
  auto [isLinearized, stride] =
      detectLinearizedStride(elementIndices, newElementType, builder, loc);

  if (isLinearized && stride) {
    return localizeLinearized(elementIndices[0], stride, builder, loc);
  }

  SmallVector<Value> indices(elementIndices.begin(), elementIndices.end());
  return localize(indices, builder, loc);
}

///===----------------------------------------------------------------------===///
/// Shared transformOps implementation (template method)
///===----------------------------------------------------------------------===///

void DbIndexerBase::transformOps(ArrayRef<Operation *> ops, Value dbPtr,
                                 Type elementType, ArtsCodegen &AC,
                                 llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbIndexerBase::transformOps with " << ops.size() << " ops");

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

    /// Handle subindex via virtual hook
    if (isa<polygeist::SubIndexOp>(op)) {
      if (handleSubIndexOp(op, dbPtr, elementType, AC, opsToRemove))
        continue;
    }

    auto access = DbUtils::getMemoryAccessInfo(op);
    if (!access)
      continue;

    ValueRange indices = access->indices;
    if (indices.empty()) {
      /// Delegate empty indices to virtual hook
      if (handleEmptyIndices(op, dbPtr, elementType, AC, opsToRemove))
        continue;
      /// If hook didn't handle it, skip
      continue;
    }

    /// Detect linearized access via virtual hook
    auto [isLinearized, stride] =
        detectLinearizedStride(indices, elementType, AC.getBuilder(), loc);

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

    if (access->isRead()) {
      auto load = cast<memref::LoadOp>(op);
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

///===----------------------------------------------------------------------===///
/// Shared transformDbRefUsers implementation (template method)
///===----------------------------------------------------------------------===///

void DbIndexerBase::transformDbRefUsers(
    DbRefOp ref, Value blockArg, Type newElementType, OpBuilder &builder,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbIndexerBase::transformDbRefUsers");

  SmallVector<Operation *> refUsers(ref.getResult().getUsers().begin(),
                                    ref.getResult().getUsers().end());
  for (Operation *refUser : refUsers) {
    OpBuilder::InsertionGuard IG(builder);
    builder.setInsertionPoint(refUser);
    Location userLoc = refUser->getLoc();

    auto access = DbUtils::getMemoryAccessInfo(refUser);
    if (!access)
      continue;
    ValueRange elementIndices = access->indices;

    if (elementIndices.empty())
      continue;

    /// Delegate localization to virtual hook (allows ElementWise to use
    /// localizeForFineGrained, Block to use standard localize)
    LocalizedIndices localized =
        localizeForDbRefUser(elementIndices, newElementType, builder, userLoc);

    auto newRef = DbRefOp::create(builder, userLoc, newElementType, blockArg,
                                          localized.dbRefIndices);

    if (access->isRead()) {
      auto load = cast<memref::LoadOp>(refUser);
      auto newLoad = memref::LoadOp::create(builder, userLoc, newRef.getResult(),
                                                    localized.memrefIndices);
      load.replaceAllUsesWith(newLoad.getResult());
      opsToRemove.insert(load);
    } else if (auto store = dyn_cast<memref::StoreOp>(refUser)) {
      memref::StoreOp::create(builder, userLoc, store.getValue(),
                                      newRef.getResult(),
                                      localized.memrefIndices);
      opsToRemove.insert(store);
    }
  }
  opsToRemove.insert(ref.getOperation());
}

///===----------------------------------------------------------------------===///
/// Shared transformUsesInParentRegion implementation
///===----------------------------------------------------------------------===///

void DbIndexerBase::transformUsesInParentRegion(
    Operation *alloc, DbAllocOp dbAlloc, ArtsCodegen &AC,
    llvm::SetVector<Operation *> &opsToRemove) {
  ARTS_DEBUG("DbIndexerBase::transformUsesInParentRegion");

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
                            << " main body accesses with indexing");

  Type elementMemRefType = dbAlloc.getAllocatedElementType();
  transformOps(users, dbAlloc.getPtr(), elementMemRefType, AC, opsToRemove);
}
