///===========================================================================
/// EDTInvariantCodeMotion
///===========================================================================

/// Dialects
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <queue>

/// Arts
#include "arts/ArtsDialect.h"
#include "arts/Transforms/EdtPointerRematerialization.h"

/// Others
#include "mlir/IR/Operation.h"
#include "llvm/ADT/SmallVector.h"

/// LLVM support
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "edt-invariant-code-motion"
#define line "-----------------------------------------\n"
#define dbgs() (llvm::dbgs())
#define DBGS() (dbgs() << "[" DEBUG_TYPE "] ")

using namespace mlir;
using namespace arts;

static inline bool isDefinedOutsideEdt(Region region, Value value) {
  return !region.isAncestor(value.getParentRegion());
};

bool canBeHoistedFromEdt(Operation *op) {
  // Do not move terminators.
  if (op->hasTrait<OpTrait::IsTerminator>())
    return false;

  // Walk the nested operations and check that all used values are defined
  // outside of the EDT region.
  auto walkFn = [&](Operation *child) {
    for (Value operand : child->getOperands()) {
      // Ignore values defined in a nested region within the op being checked.
      // Example: An scf.if inside the EDT region. Operands defined inside the
      // scf.if's regions should not prevent hoisting the scf.if itself if its
      // condition is invariant.
      if (op->isAncestor(operand.getParentRegion()->getParentOp()))
        continue;
      if (!definedOutside(operand))
        return WalkResult::interrupt();
    }
    return WalkResult::advance();
  };
  return !op->walk(walkFn).wasInterrupted();
}

uint64_t moveEdtInvariantCode(arts::EdtOp edtOp) {
  Region &region = edtOp.getRegion();
  /// An EDT region is expected to have a single block.
  if (region.empty() || region.front().empty()) {
    /// Nothing to move if region or its block is empty
    return 0;
  }

  Block &body = region.front();
  uint64_t numMoved = 0;

  /// Define the callback to check if an operation should be moved out.
  auto shouldMoveOutOfEdt = [&](Operation *op) {
    /// Basic checks: not a terminator, memory effect free
    if (op->hasTrait<OpTrait::IsTerminator>() || !mlir::isMemoryEffectFree(op))
      return false;

    /// Do not move other ARTS ops
    if (isa<arts::ArtsDialect>(op->getDialect()))
      return false;

    return true;
  };

  /// Define the callback to actually move the operation.
  auto moveOutOfEdt = [&](Operation *op) {
    /// Move the operation before the EdtOp itself.
    op->moveBefore(edtOp);
  };

  std::queue<Operation *> worklist;
  /// Add top-level operations in the EDT body to the worklist.
  /// Iterate backwards for safe removal/moving while iterating.
  for (Operation &op : llvm::reverse(body.getOperations())) {
    worklist.push(&op);
  }

  /// Process the worklist
  llvm::SmallPtrSet<Operation *, 16> visitedOrMoved;

  while (!worklist.empty()) {
    Operation *op = worklist.front();
    worklist.pop();

    /// Skip ops that have already been moved or processed. Check if the op is
    /// still within the EDT's region
    if (!op->getParentRegion() || op->getParentRegion() != &region ||
        visitedOrMoved.count(op))
      continue;

    visitedOrMoved.insert(op);

    /// Check if the op should be moved and can be hoisted
    LLVM_DEBUG(dbgs() << "Checking op: " << *op << "\n");
    if (shouldMoveOutOfEdt(op) && canBeHoistedFromEdt(op, region)) {
      LLVM_DEBUG(dbgs() << "Moving EDT-invariant op: " << *op << "\n");
      moveOutOfEdt(op);
      ++numMoved;

      /// Add users of the moved op (that are still inside the region)
      /// back to the worklist, as they might now become hoistable.
      /// Need a copy of users as the user list might change during iteration.
      SmallVector<Operation *> users(op->getUsers());
      for (Operation *user : users) {
        /// Check if user is still inside the original region and not yet
        /// processed
        if (user->getParentRegion() == &region &&
            visitedOrMoved.find(user) == visitedOrMoved.end()) {
          /// Re-add to worklist for potential hoisting
          worklist.push(user);
        }
      }
    }
  }

  return numMoved;
}