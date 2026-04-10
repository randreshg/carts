///==========================================================================///
/// File: EdtUtils.h
///
/// Utility functions and EdtEnvManager for working with ARTS EDTs (EdtOp).
/// Contains EdtEnvManager (capture classification), IR manipulation helpers
/// (block splicing, epoch wrapping, block argument navigation), and
/// consecutive-op fusion.
///==========================================================================///

#ifndef CARTS_UTILS_EDTUTILS_H
#define CARTS_UTILS_EDTUTILS_H

#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/edt/EdtInfo.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Block.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
class BlockArgument;
}

namespace mlir {
namespace arts {

/// Forward declarations
class EdtOp;
class EpochOp;
class DbAcquireOp;

/// Forward-declare so EdtEnvManager::analyze() can call it inline.
void analyzeEdtCapturedValues(EdtOp edt, llvm::SetVector<Value> &capturedValues,
                              llvm::SetVector<Value> &parameters,
                              llvm::SetVector<Value> &constants,
                              llvm::SetVector<Value> &dbHandles);

///===----------------------------------------------------------------------===//
/// EdtEnvManager
///===----------------------------------------------------------------------===//

/// Manages EDT environment classification: captured values are partitioned
/// into parameters, constants, DB handles, and dependencies.  This is the
/// canonical helper for any pass that needs to reason about what an EDT
/// captures from its enclosing scope (EdtLowering, EpochOptCpsChain, etc.).
class EdtEnvManager {
public:
  EdtEnvManager(EdtOp edtOp) : edtOp(edtOp) { analyze(); }
  EdtEnvManager(EdtOp edtOp, const EdtCaptureSummary &captureSummary)
      : edtOp(edtOp) {
    loadCaptureSummary(captureSummary);
  }

  void analyze() {
    analyzeEdtCapturedValues(edtOp, capturedValues, parameters, constants,
                             dbHandles);
    for (Value operand : edtOp.getDependencies())
      deps.insert(operand);
  }

  ArrayRef<Value> getParameters() const { return parameters.getArrayRef(); }
  ArrayRef<Value> getConstants() const { return constants.getArrayRef(); }
  ArrayRef<Value> getCapturedValues() const {
    return capturedValues.getArrayRef();
  }
  ArrayRef<Value> getDependencies() const { return deps.getArrayRef(); }
  ArrayRef<Value> getDbHandles() const { return dbHandles.getArrayRef(); }
  const DenseMap<Value, unsigned> &getValueToPackIndex() const {
    return valueToPackIndex;
  }
  DenseMap<Value, unsigned> &getValueToPackIndex() { return valueToPackIndex; }

private:
  void loadCaptureSummary(const EdtCaptureSummary &captureSummary) {
    auto appendValues = [](SetVector<Value> &dst, ArrayRef<Value> values) {
      for (Value value : values)
        dst.insert(value);
    };
    appendValues(capturedValues, captureSummary.capturedValues);
    appendValues(parameters, captureSummary.parameters);
    appendValues(constants, captureSummary.constants);
    appendValues(dbHandles, captureSummary.dbHandles);
    appendValues(deps, captureSummary.dependencies);
  }

  EdtOp edtOp;
  SetVector<Value> capturedValues, parameters, constants, deps, dbHandles;
  DenseMap<Value, unsigned> valueToPackIndex;
};

///===----------------------------------------------------------------------===//
/// Inline Helpers
///===----------------------------------------------------------------------===//

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

/// Return true when an EDT is nested inside an EpochOp.
inline bool isInsideEpoch(EdtOp op) {
  return op->getParentOfType<EpochOp>() != nullptr;
}

/// Return true when an operation is nested inside an EpochOp.
inline bool isInsideEpoch(Operation *op) {
  return op->getParentOfType<EpochOp>() != nullptr;
}

/// Return the single top-level arts::ForOp inside an EDT body, or nullptr
/// if there are zero or multiple top-level ForOps, or any non-ForOp
/// non-terminator operation exists.
ForOp getSingleTopLevelFor(EdtOp edt);

/// Collect all top-level arts::ForOp operations in an EDT body (ignoring
/// non-ForOp operations).  Returns an empty vector when \p edt is null.
SmallVector<ForOp, 2> getTopLevelForOps(EdtOp edt);

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

/// Classify an explicit ordered list of EDT user values using the same
/// parameter/constant/DB-handle contract as EDT lowering.
void classifyEdtUserValues(ArrayRef<Value> userValues,
                           llvm::SetVector<Value> &parameters,
                           llvm::SetVector<Value> &constants,
                           llvm::SetVector<Value> &dbHandles);

/// Analyze the values an EDT captures from above its region using the same
/// classification contract as EDT lowering.
void analyzeEdtCapturedValues(EdtOp edt, llvm::SetVector<Value> &capturedValues,
                              llvm::SetVector<Value> &parameters,
                              llvm::SetVector<Value> &constants,
                              llvm::SetVector<Value> &dbHandles);

/// Collect values in the same logical order EdtLowering packs them into
/// `arts.edt_param_pack`: user params, DB handles, then dep-derived scalars
/// (indices/offsets/sizes/partition slices/element sizes). DB handles are
/// returned in their original SSA form; lowering is responsible for casting
/// them to raw i64 payloads.
SmallVector<Value> collectEdtPackedValues(EdtOp edt);

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_EDTUTILS_H
