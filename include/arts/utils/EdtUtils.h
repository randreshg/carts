///==========================================================================///
/// File: EdtUtils.h
///
/// Utility functions for working with ARTS EDTs (EdtOp).
/// Contains IR manipulation helpers: block splicing, epoch wrapping,
/// block argument navigation, and consecutive-op fusion.
///==========================================================================///

#ifndef CARTS_UTILS_EDTUTILS_H
#define CARTS_UTILS_EDTUTILS_H

#include "arts/Dialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/STLExtras.h"

namespace mlir {
class BlockArgument;
}

namespace mlir {
namespace arts {

/// Forward declarations
class EdtOp;
class EpochOp;
class DbAcquireOp;

/// Move all non-terminator operations from src block into dst block,
/// inserting them before dst's terminator.
inline void spliceBodyBeforeTerminator(Block &src, Block &dst) {
  Operation *terminator = dst.getTerminator();
  for (Operation &op : llvm::make_early_inc_range(src.without_terminator()))
    op.moveBefore(terminator);
}

/// Fuse consecutive pairs of operations of type OpT in a block.
/// canFuse checks if two consecutive ops can be fused.
/// doFuse performs the fusion (must erase or modify the second op).
/// Returns true if any fusion was performed.
template <typename OpT>
bool fuseConsecutivePairs(Block &block,
                          llvm::function_ref<bool(OpT, OpT)> canFuse,
                          llvm::function_ref<void(OpT, OpT)> doFuse) {
  bool changed = false;
  for (auto it = block.begin(); it != block.end();) {
    auto first = dyn_cast<OpT>(&*it);
    if (!first) {
      ++it;
      continue;
    }
    auto nextIt = std::next(it);
    if (nextIt == block.end()) {
      ++it;
      continue;
    }
    auto second = dyn_cast<OpT>(&*nextIt);
    if (!second) {
      ++it;
      continue;
    }
    if (!canFuse(first, second)) {
      ++it;
      continue;
    }
    doFuse(first, second);
    changed = true;
    /// Re-run on same first op to chain-fuse.
  }
  return changed;
}

/// Return the single top-level arts::ForOp inside an EDT body, or nullptr
/// if there are zero or multiple top-level ForOps, or any non-ForOp
/// non-terminator operation exists.
ForOp getSingleTopLevelFor(EdtOp edt);

/// Wrap all operations (except terminator) in a block inside an EpochOp.
/// Returns the created EpochOp, or nullptr if no operations to wrap.
EpochOp wrapBodyInEpoch(Block &body, Location loc);

/// Finds the EdtOp that uses a DbAcquireOp and returns the corresponding
/// block argument. Returns {EdtOp, BlockArgument} pair, or {nullptr,
/// BlockArgument()} if not found.
std::pair<EdtOp, BlockArgument>
getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp);

/// Map a memref value back to its EDT block argument index by stripping
/// view-like wrapper ops and DbRefOp.
std::optional<unsigned> mapMemrefToEdtArg(EdtOp edt, Value memrefValue);

/// Classify each EDT dependency as read, written, or both by walking all
/// load/store operations in the EDT body and tracing accessed memrefs
/// back to their block arguments.
void classifyEdtArgAccesses(EdtOp edt, SmallVectorImpl<bool> &reads,
                            SmallVectorImpl<bool> &writes);

/// Return true when an alloca initialization store can be cloned into an EDT
/// body without needing its surrounding control flow. This accepts constant or
/// pure regionless operand chains whose inputs can be captured by the EDT.
bool canCloneAllocaInitStore(memref::StoreOp store, Value memref);

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_EDTUTILS_H
