///==========================================================================///
/// File: OpRemovalManager.cpp
/// Implements the OpRemovalManager class for deferred operation removal.
///==========================================================================///

#include "arts/Utils/OpRemovalManager.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/ArtsDebug.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"

ARTS_DEBUG_SETUP(op_removal_manager);

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
/// OpRemovalManager Implementation
///===----------------------------------------------------------------------===///

void OpRemovalManager::markForRemoval(Operation *op) {
  if (!op)
    return;
  if (isMarkedForRemoval(op))
    return;

  /// If an ancestor is already scheduled for removal, no need to track this
  /// operation - it will be removed when the ancestor is removed
  for (Operation *parent = op->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (isMarkedForRemoval(parent))
      return;
  }

  /// Remove any descendants that are already scheduled; removing this op will
  /// remove them as well
  SmallVector<Operation *, 4> descendants;
  for (Operation *scheduled : opsToRemove) {
    if (op->isAncestor(scheduled) && scheduled != op)
      descendants.push_back(scheduled);
  }
  for (Operation *scheduled : descendants)
    opsToRemove.remove(scheduled);

  opsToRemove.insert(op);
}

bool OpRemovalManager::isMarkedForRemoval(Operation *op) const {
  return opsToRemove.contains(op);
}

LogicalResult OpRemovalManager::verifyAllUsersMarked(Value value,
                                                     StringRef allocName) {
  unsigned totalUsers = 0;
  SmallVector<Operation *> unmarkedUsers;

  for (Operation *user : value.getUsers()) {
    totalUsers++;
    if (!isMarkedForRemoval(user))
      unmarkedUsers.push_back(user);
  }

  if (!unmarkedUsers.empty()) {
    if (!allocName.empty()) {
      ARTS_DEBUG("  Warning: " << allocName << " has " << unmarkedUsers.size()
                               << " unmarked users out of " << totalUsers);
    } else {
      ARTS_DEBUG("  Warning: Value has " << unmarkedUsers.size()
                                         << " unmarked users out of "
                                         << totalUsers);
    }
    for (Operation *user : unmarkedUsers) {
      ARTS_DEBUG("    - Unmarked: " << user->getName().getStringRef());
    }
    return failure();
  }

  return success();
}

void OpRemovalManager::removeAllMarked() {
  /// Remove all operations marked for removal
  /// Only remove operations that have no uses and haven't been erased already
  for (Operation *op : opsToRemove) {
    /// Check if operation is still alive (not already erased as a child)
    /// If the operation has been erased, its parent will be null
    if (op && op->getBlock() && op->use_empty()) {
      op->erase();
    }
  }
  opsToRemove.clear();
}

///===----------------------------------------------------------------------===///
/// Recursive Removal Implementation
///===----------------------------------------------------------------------===///

void OpRemovalManager::removeOpImpl(Operation *op, OpBuilder &builder,
                                    SmallPtrSet<Operation *, 32> &seen,
                                    bool recursive) {
  /// Check if operation is still valid
  if (!op)
    return;

  /// Check if operation is still properly linked in the IR
  Block *block = op->getBlock();
  if (!block)
    return;

  /// Additional check: ensure the block has a valid parent operation
  Operation *parentOp = block->getParentOp();
  if (!parentOp)
    return;

  /// Already visited
  if (!seen.insert(op).second)
    return;

  ARTS_DEBUG("   - Removing operation: " << *op);

  /// Get dependents before modifying uses
  SmallVector<Operation *, 8> dependents;
  if (recursive) {
    for (Value result : op->getResults())
      for (Operation *user : result.getUsers())
        dependents.push_back(user);
  }

  /// If this is a terminator, mark its parent for removal
  if (op->hasTrait<OpTrait::IsTerminator>()) {
    if (Operation *parent = op->getParentOp())
      markForRemoval(parent);
  }

  /// Drop all uses of this operation's results to dereference them
  op->dropAllUses();

  /// Erase the operation immediately since all uses have been dropped
  op->erase();

  /// No recursion requested
  if (!recursive)
    return;

  /// Remove dependents recursively
  for (Operation *userOp : dependents)
    removeOpImpl(userOp, builder, seen, recursive);
}

void OpRemovalManager::removeAllMarked(ModuleOp module, bool recursive) {
  if (opsToRemove.empty())
    return;

  OpBuilder builder(module.getContext());
  SmallPtrSet<Operation *, 32> seen;

  /// Remove operations, processing newly discovered dead parents
  ARTS_DEBUG(" - Removing " << opsToRemove.size() << " marked operations");

  /// Filter out operations that may have been removed
  SetVector<Operation *> validOpsToRemove;
  for (Operation *op : opsToRemove) {
    if (op && op->getBlock() && op->getBlock()->getParentOp())
      validOpsToRemove.insert(op);
  }

  /// Process operations
  for (Operation *op : validOpsToRemove) {
    removeOpImpl(op, builder, seen, recursive);
  }

  opsToRemove.clear();
  ARTS_DEBUG(" - Removed " << seen.size() << " operations total");
}

///===----------------------------------------------------------------------===///
/// Static Utility Methods
///===----------------------------------------------------------------------===///

void OpRemovalManager::removeUndefOps(ModuleOp module) {
  OpRemovalManager mgr;
  module.walk([&](arts::UndefOp op) { mgr.markForRemoval(op); });
  ARTS_DEBUG(" - Removing " << mgr.size() << " undef operations");
  mgr.removeAllMarked(module, /*recursive=*/true);
}

void OpRemovalManager::replaceWithUndef(Operation *op, OpBuilder &builder) {
  if (!op || op->getNumResults() == 0)
    return;

  OpBuilder::InsertionGuard IG(builder);
  builder.setInsertionPoint(op);
  for (Value result : op->getResults()) {
    if (!result.use_empty()) {
      auto undef =
          builder.create<arts::UndefOp>(op->getLoc(), result.getType());
      result.replaceAllUsesWith(undef.getResult());
    }
  }
}

///===----------------------------------------------------------------------===///
/// Helper Functions Implementation
///===----------------------------------------------------------------------===///

bool isInsideLoop(Operation *op) {
  if (!op)
    return false;
  return op->getParentOfType<scf::ForOp>() ||
         op->getParentOfType<affine::AffineForOp>();
}

SmallVector<Operation *> collectLoads(Value memref) {
  SmallVector<Operation *> loads;
  for (Operation *user : memref.getUsers()) {
    if (isa<memref::LoadOp, affine::AffineLoadOp>(user))
      loads.push_back(user);
  }
  return loads;
}

SmallVector<Operation *> collectStores(Value memref) {
  SmallVector<Operation *> stores;
  for (Operation *user : memref.getUsers()) {
    if (isa<memref::StoreOp, affine::AffineStoreOp>(user))
      stores.push_back(user);
  }
  return stores;
}

} // namespace arts
} // namespace mlir
