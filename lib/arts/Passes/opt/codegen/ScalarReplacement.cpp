///==========================================================================///
/// File: ScalarReplacement.cpp
///
/// This pass transforms memory-based reduction patterns in scf.for loops
/// to register-based iter_args patterns to enable LLVM vectorization.
///
/// Before (blocks vectorization due to loop-carried memory dependency):
///   scf.for %i = ... {
///     %old = memref.load %acc[...]
///     %new = arith.addf %old, %val
///     memref.store %new, %acc[...]
///   }
///
/// After (enables vectorization):
///   %init = memref.load %acc[...]
///   %final = scf.for %i = ... iter_args(%acc_reg = %init) {
///     %new = arith.addf %acc_reg, %val
///     scf.yield %new
///   }
///   memref.store %final, %acc[...]
///
///==========================================================================///

#include "../../PassDetails.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "polygeist/Ops.h"

ARTS_DEBUG_SETUP(arts_scalar_replacement);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Information about a detected reduction pattern
struct ReductionPattern {
  scf::ForOp forOp;
  Operation *loadOp;          /// memref.load or polygeist.load
  Operation *storeOp;         /// memref.store or polygeist.store
  Operation *reduceOp;        /// arith.addf/mulf/etc.
  Value memref;               /// The memref being accessed
  SmallVector<Value> indices; /// Loop-invariant indices
};

/// Check if a value is loop-invariant (not dependent on the induction variable)
static bool isLoopInvariant(Value val, scf::ForOp forOp) {
  /// Block argument of the loop (induction variable) is NOT invariant
  if (auto blockArg = dyn_cast<BlockArgument>(val)) {
    if (blockArg.getOwner() == forOp.getBody())
      return false;
  }

  /// Check if defining op is inside the loop
  if (Operation *defOp = val.getDefiningOp()) {
    if (forOp.getBody()->findAncestorOpInBlock(*defOp))
      return false;
  }

  return true;
}

/// Check if all indices are loop-invariant
static bool areIndicesLoopInvariant(ArrayRef<Value> indices, scf::ForOp forOp) {
  for (Value idx : indices) {
    if (!isLoopInvariant(idx, forOp))
      return false;
  }
  return true;
}

/// Get the memref and indices from a load operation
static std::pair<Value, SmallVector<Value>> getLoadInfo(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return {load.getMemRef(), SmallVector<Value>(load.getIndices())};

  /// Handle polygeist.load (DynLoadOp)
  if (auto dynLoad = dyn_cast<polygeist::DynLoadOp>(op))
    return {dynLoad.getMemref(), SmallVector<Value>(dynLoad.getIndices())};
  return {Value(), {}};
}

/// Get the memref and indices from a store operation
static std::pair<Value, SmallVector<Value>> getStoreInfo(Operation *op) {
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return {store.getMemRef(), SmallVector<Value>(store.getIndices())};

  /// Handle polygeist.store (DynStoreOp)
  if (auto dynStore = dyn_cast<polygeist::DynStoreOp>(op))
    return {dynStore.getMemref(), SmallVector<Value>(dynStore.getIndices())};
  return {Value(), {}};
}

/// Get the value being stored
static Value getStoredValue(Operation *op) {
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getValue();
  if (auto dynStore = dyn_cast<polygeist::DynStoreOp>(op))
    return dynStore.getValue();
  return Value();
}

/// Check if two index vectors are equal
static bool indicesEqual(ArrayRef<Value> a, ArrayRef<Value> b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

/// Check if an operation is an accumulation operation (addf, mulf, etc.)
static bool isAccumulationOp(Operation *op) {
  return isa<arith::AddFOp, arith::MulFOp, arith::AddIOp, arith::MulIOp,
             arith::MaximumFOp, arith::MinimumFOp>(op);
}

/// Check if an operation is a direct child of a block (not inside nested
/// regions)
static bool isDirectChildOf(Operation *op, Block *block) {
  return op->getBlock() == block;
}

/// Detect reduction pattern in a for loop
static std::optional<ReductionPattern>
detectReductionPattern(scf::ForOp forOp) {
  ReductionPattern pattern;
  pattern.forOp = forOp;

  Block *body = forOp.getBody();

  /// Collect loads and stores that are DIRECT children of the loop body
  /// (not inside nested regions - those need to be transformed first)
  SmallVector<Operation *> loads, stores;
  for (Operation &op : body->without_terminator()) {
    if (isa<memref::LoadOp, polygeist::DynLoadOp>(&op))
      loads.push_back(&op);
    else if (isa<memref::StoreOp, polygeist::DynStoreOp>(&op))
      stores.push_back(&op);
  }

  /// For each store, check if there's a matching load with loop-invariant
  /// indices
  for (Operation *storeOp : stores) {
    auto [storeMemref, storeIndices] = getStoreInfo(storeOp);
    if (!storeMemref || storeIndices.empty())
      continue;

    /// Check if indices are loop-invariant
    if (!areIndicesLoopInvariant(storeIndices, forOp))
      continue;

    Value storedValue = getStoredValue(storeOp);
    if (!storedValue)
      continue;

    /// Check if stored value comes from an accumulation operation
    Operation *accOp = storedValue.getDefiningOp();
    if (!accOp)
      continue;

    /// The accumulation op must also be a direct child of the loop body
    if (!isDirectChildOf(accOp, body))
      continue;

    if (!isAccumulationOp(accOp))
      continue;

    /// Check if one operand of the accumulation is loaded from the same
    /// location
    for (Value operand : accOp->getOperands()) {
      Operation *loadOp = operand.getDefiningOp();
      if (!loadOp)
        continue;

      auto [loadMemref, loadIndices] = getLoadInfo(loadOp);
      if (!loadMemref)
        continue;

      /// Check if load is from the same location as store.
      /// Also ensure the load has only one use (the accumulation op)
      /// to avoid dangling references when erasing the old loop.
      /// Additionally, the memref must be defined OUTSIDE the loop so that
      /// we can create new loads/stores outside the loop that use it.
      if (loadMemref == storeMemref &&
          indicesEqual(loadIndices, storeIndices) && loadOp->hasOneUse() &&
          isLoopInvariant(storeMemref, forOp)) {
        pattern.loadOp = loadOp;
        pattern.storeOp = storeOp;
        pattern.reduceOp = accOp;
        pattern.memref = storeMemref;
        pattern.indices = storeIndices;

        ARTS_DEBUG("Found reduction pattern in loop at " << forOp.getLoc());

        return pattern;
      }
    }
  }

  return std::nullopt;
}

/// Transform a reduction pattern to use iter_args
static LogicalResult transformReduction(ReductionPattern &pattern,
                                        IRRewriter &rewriter) {
  scf::ForOp forOp = pattern.forOp;
  Location loc = forOp.getLoc();

  /// Load initial value before the loop
  rewriter.setInsertionPoint(forOp);

  Value initValue;
  if (auto load = dyn_cast<memref::LoadOp>(pattern.loadOp)) {
    initValue = rewriter.create<memref::LoadOp>(loc, load.getMemRef(),
                                                load.getIndices());
  } else if (auto dynLoad = dyn_cast<polygeist::DynLoadOp>(pattern.loadOp)) {
    /// For polygeist.load (DynLoadOp), create new load before the loop
    initValue = rewriter.create<polygeist::DynLoadOp>(
        loc, dynLoad.getResult().getType(), dynLoad.getMemref(),
        dynLoad.getIndices(), dynLoad.getSizes());
  }

  /// Create new ForOp with iter_args
  SmallVector<Value> iterArgs = {initValue};
  auto newForOp = rewriter.create<scf::ForOp>(loc, forOp.getLowerBound(),
                                              forOp.getUpperBound(),
                                              forOp.getStep(), iterArgs);

  /// Copy attributes (like arts.loop metadata)
  newForOp->setAttrs(forOp->getAttrs());

  /// Get the accumulator iter_arg
  Value accArg = newForOp.getRegionIterArg(0);

  /// Clone the loop body, replacing load with iter_arg
  Block *oldBody = forOp.getBody();
  Block *newBody = newForOp.getBody();

  /// Map old values to new values
  IRMapping mapper;
  mapper.map(forOp.getInductionVar(), newForOp.getInductionVar());
  mapper.map(pattern.loadOp->getResult(0), accArg);

  /// Erase the auto-generated yield terminator.
  if (!newBody->empty()) {
    Operation *existingTerminator = &newBody->back();
    if (existingTerminator &&
        existingTerminator->hasTrait<OpTrait::IsTerminator>())
      rewriter.eraseOp(existingTerminator);
  }

  rewriter.setInsertionPointToEnd(newBody);

  /// Clone operations except the load and store
  Value newAccValue;
  for (Operation &op : oldBody->without_terminator()) {
    if (&op == pattern.loadOp)
      continue; /// Skip the load - replaced by iter_arg
    if (&op == pattern.storeOp)
      continue; /// Skip the store - replaced by yield

    Operation *cloned = rewriter.clone(op, mapper);

    /// Track the accumulation result
    if (&op == pattern.reduceOp)
      newAccValue = cloned->getResult(0);
  }

  /// Create yield with the accumulated value
  if (!newAccValue) {
    /// This can happen if reduceOp is inside a nested region - shouldn't reach
    /// here with our detection checks, but guard against it anyway
    return failure();
  }
  rewriter.create<scf::YieldOp>(loc, ValueRange{newAccValue});

  /// Store final result after the loop
  rewriter.setInsertionPointAfter(newForOp);
  Value finalValue = newForOp.getResult(0);

  if (auto store = dyn_cast<memref::StoreOp>(pattern.storeOp)) {
    rewriter.create<memref::StoreOp>(loc, finalValue, store.getMemRef(),
                                     store.getIndices());
  } else if (auto dynStore = dyn_cast<polygeist::DynStoreOp>(pattern.storeOp)) {
    /// For polygeist.store (DynStoreOp), create new store with updated value
    rewriter.create<polygeist::DynStoreOp>(
        loc, finalValue, dynStore.getMemref(), dynStore.getIndices(),
        dynStore.getSizes());
  }

  /// Erase the old loop
  rewriter.eraseOp(forOp);

  ARTS_INFO("Transformed reduction pattern to iter_args at " << loc);

  return success();
}

struct ScalarReplacementPass
    : public ScalarReplacementBase<ScalarReplacementPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    ARTS_INFO_HEADER(ScalarReplacementPass);

    int transformed = 0;

    /// Walk all scf.for loops and try to transform reductions
    /// Do this iteratively as transformations change the IR
    bool changed = true;
    while (changed) {
      changed = false;

      SmallVector<scf::ForOp> forOps;
      module.walk([&](scf::ForOp forOp) { forOps.push_back(forOp); });

      for (scf::ForOp forOp : forOps) {
        /// Skip loops that already have iter_args
        if (!forOp.getInitArgs().empty())
          continue;

        auto pattern = detectReductionPattern(forOp);
        if (!pattern)
          continue;

        /// Transform the pattern
        IRRewriter rewriter(module.getContext());
        if (succeeded(transformReduction(*pattern, rewriter))) {
          transformed++;
          changed = true;
          break; /// Start over after transformation
        }
      }
    }

    ARTS_INFO("Transformed " << transformed << " reduction patterns");
    ARTS_INFO_FOOTER(ScalarReplacementPass);
  }
};

} // namespace

///===----------------------------------------------------------------------===///
/// Pass creation and registration
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createScalarReplacementPass() {
  return std::make_unique<ScalarReplacementPass>();
}

} // namespace arts
} // namespace mlir
