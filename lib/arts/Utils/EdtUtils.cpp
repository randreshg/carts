///==========================================================================///
/// File: EdtUtils.cpp
///
/// Implementation of utility functions for working with ARTS EDTs.
///==========================================================================///

#include "arts/Utils/EdtUtils.h"
#include "arts/Utils/ValueUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <algorithm>
#include <functional>
#include <optional>

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(edt_utils);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// EDT Analysis Utilities
///===----------------------------------------------------------------------===///

/// Check if a block argument is invariant with respect to an EDT region.
static bool isBlockArgInvariant(Region &edtRegion, BlockArgument blockArg) {
  Block *owner = blockArg.getOwner();
  if (!owner)
    return false;
  Region *ownerRegion = owner->getParent();
  if (!ownerRegion)
    return false;

  /// Entry block arguments of the EDT region are considered invariant
  /// because their defining value is outside the region.
  if (ownerRegion == &edtRegion && owner == &edtRegion.front())
    return true;

  /// Block arguments that belong to regions outside of this EDT are also
  /// treated as invariant inputs.
  return !edtRegion.isAncestor(ownerRegion);
}

/// Check if an operation's result is invariant with respect to an EDT region.
/// Recursive helper for isInvariantInEdt.
static bool isDefOpInvariant(Region &edtRegion, Value v,
                             SmallPtrSetImpl<Value> &visited) {
  if (!v)
    return false;
  if (!visited.insert(v).second)
    return true;

  if (ValueUtils::isValueConstant(v))
    return true;

  if (auto blockArg = v.dyn_cast<BlockArgument>())
    return isBlockArgInvariant(edtRegion, blockArg);

  Operation *defOp = v.getDefiningOp();
  if (!defOp)
    return false;

  if (!edtRegion.isAncestor(defOp->getParentRegion()))
    return true;

  if (isa<arith::ConstantIndexOp, arith::ConstantIntOp>(defOp))
    return true;

  if (isa<arith::AddIOp, arith::SubIOp, arith::MulIOp, arith::DivSIOp,
          arith::DivUIOp, arith::IndexCastOp, arith::ExtSIOp, arith::ExtUIOp,
          arith::TruncIOp>(defOp)) {
    for (Value operand : defOp->getOperands())
      if (!isDefOpInvariant(edtRegion, operand, visited))
        return false;
    return true;
  }

  return false;
}

bool EdtUtils::isInvariantInEdt(Region &edtRegion, Value value) {
  SmallPtrSet<Value, 16> visited;

  if (!isDefOpInvariant(edtRegion, value, visited))
    return false;

  /// Check for pointer-like types and their users
  if (!value.getType().isa<MemRefType, UnrankedMemRefType>())
    return true;

  for (Operation *user : value.getUsers()) {
    if (!edtRegion.isAncestor(user->getParentRegion()))
      continue;

    if (auto store = dyn_cast<memref::StoreOp>(user))
      if (store.getMemref() == value)
        return false;

    if (auto atomicRmw = dyn_cast<memref::AtomicRMWOp>(user))
      if (atomicRmw.getMemref() == value)
        return false;
  }

  return true;
}

bool EdtUtils::isReachable(Operation *source, Operation *target) {
  /// Early exit if either pointer is null or both are the same.
  if (!source || !target)
    return false;
  if (source == target)
    return true;

  /// If either op is detached, conservatively report not reachable.
  if (!source->getBlock() || !target->getBlock())
    return false;

  /// If both operations are in the same block, check their order.
  if (source->getBlock() == target->getBlock())
    return source->isBeforeInBlock(target);

  /// Get the EDT parent for both operations. This acts as our upper limit.
  auto srcEdt = source->getParentOfType<EdtOp>();
  auto tgtEdt = target->getParentOfType<EdtOp>();
  if (srcEdt != tgtEdt)
    return false;

  /// Traverse up the parent chain simultaneously but stop once the EDT parent
  /// is reached.
  Operation *src = source;
  Operation *tgt = target;
  while (true) {
    if (src->getBlock() == tgt->getBlock())
      return src->isBeforeInBlock(tgt);
    if (src == srcEdt && tgt == tgtEdt)
      break;
    if (src->getParentOp() != srcEdt)
      src = src->getParentOp();
    if (tgt->getParentOp() != tgtEdt)
      tgt = tgt->getParentOp();
  }
  return false;
}

///===----------------------------------------------------------------------===///
/// EDT Block Argument Utilities
///===----------------------------------------------------------------------===///

std::pair<EdtOp, BlockArgument>
EdtUtils::getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp) {
  /// Find the EDT that uses this acquire's pointer result
  EdtOp edtUser = nullptr;
  Value operandValue = nullptr;

  for (auto &use : acquireOp->getUses()) {
    Operation *userOp = use.getOwner();
    if (auto edtOp = dyn_cast<EdtOp>(userOp)) {
      edtUser = edtOp;
      operandValue = edtOp->getOperand(use.getOperandNumber());
      break;
    }
  }

  if (!edtUser || !operandValue)
    return {nullptr, nullptr};

  /// Find the operand Value in the dependencies range
  /// The index within dependencies equals the block argument index
  ValueRange deps = edtUser.getDependencies();
  auto depIt = std::find(deps.begin(), deps.end(), operandValue);
  if (depIt == deps.end())
    return {nullptr, nullptr};

  unsigned blockArgIdx = std::distance(deps.begin(), depIt);

  /// Get the block argument
  Block &body = edtUser.getRegion().front();
  if (blockArgIdx >= body.getNumArguments())
    return {nullptr, nullptr};

  BlockArgument blockArg = body.getArgument(blockArgIdx);
  return {edtUser, blockArg};
}

///===----------------------------------------------------------------------===///
/// EDT Transformation Utilities
///===----------------------------------------------------------------------===///

bool EdtUtils::hasNestedEdt(EdtOp edt) {
  bool found = false;
  edt.getRegion().walk([&](EdtOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

EpochOp EdtUtils::wrapBodyInEpoch(Block &body, Location loc) {
  SmallVector<Operation *, 8> opsToMove;
  for (Operation &op : body.without_terminator())
    opsToMove.push_back(&op);

  if (opsToMove.empty())
    return nullptr;

  OpBuilder builder(body.getTerminator());
  auto epochOp = builder.create<EpochOp>(loc);
  auto &epochBlock = epochOp.getRegion().emplaceBlock();

  for (Operation *op : opsToMove)
    op->moveBefore(&epochBlock, epochBlock.end());

  builder.setInsertionPointToEnd(&epochBlock);
  builder.create<YieldOp>(loc);

  return epochOp;
}
