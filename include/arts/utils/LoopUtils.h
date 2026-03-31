///==========================================================================///
/// File: LoopUtils.h
///
/// Utility functions for querying SCF loop properties.
/// Lightweight inline helpers for worker-loop detection, innermost-loop
/// checks, and bound matching — complements the heavier LoopAnalysis
/// framework without requiring an AnalysisManager.
///==========================================================================///

#ifndef ARTS_UTILS_LOOPUTILS_H
#define ARTS_UTILS_LOOPUTILS_H

#include "arts/Dialect.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include <optional>

namespace mlir {
namespace arts {

enum class PointwiseLoopComputeClass { arithmeticOnly, vectorMath, scalarCall };

/// Check whether a scf::ForOp is a "worker loop" (i.e., contains at least one
/// arts.edt operation anywhere in its body).
/// Used by epoch-level passes to identify task-spawning loops.
inline bool isWorkerLoop(scf::ForOp loop) {
  bool hasEdt = false;
  loop.walk([&](EdtOp) {
    hasEdt = true;
    return WalkResult::interrupt();
  });
  return hasEdt;
}

/// Check whether a scf::ForOp is the innermost loop (contains no nested
/// scf::ForOp operations). Used by strip-mining and other loop transforms
/// that target leaf loops only.
inline bool isInnermostLoop(scf::ForOp loop) {
  bool hasNested = false;
  loop.getBody()->walk([&](scf::ForOp nested) {
    if (nested != loop) {
      hasNested = true;
      return WalkResult::interrupt();
    }
    return WalkResult::advance();
  });
  return !hasNested;
}

/// Check whether two scf::ForOp loops have matching bounds (same lower
/// bound, upper bound, and step). Uses ValueAnalysis::sameValue for comparison.
/// Note: this overload is for scf::ForOp only; arts::ForOp has a separate
/// bounds API and is handled in LoopFusion directly.
inline bool haveCompatibleBounds(scf::ForOp a, scf::ForOp b) {
  return ValueAnalysis::sameValue(a.getLowerBound(), b.getLowerBound()) &&
         ValueAnalysis::sameValue(a.getUpperBound(), b.getUpperBound()) &&
         ValueAnalysis::sameValue(a.getStep(), b.getStep());
}

/// Check whether two arts::ForOp loops have the same iteration space (same
/// lower bound, upper bound, and step). Uses ValueAnalysis::sameValue for
/// comparison. Returns false if either loop is null or has multi-dimensional
/// bounds.
inline bool haveSameBounds(ForOp a, ForOp b) {
  if (!a || !b)
    return false;
  if (a.getLowerBound().size() != 1 || a.getUpperBound().size() != 1 ||
      a.getStep().size() != 1 || b.getLowerBound().size() != 1 ||
      b.getUpperBound().size() != 1 || b.getStep().size() != 1)
    return false;
  return ValueAnalysis::sameValue(a.getLowerBound().front(),
                                  b.getLowerBound().front()) &&
         ValueAnalysis::sameValue(a.getUpperBound().front(),
                                  b.getUpperBound().front()) &&
         ValueAnalysis::sameValue(a.getStep().front(), b.getStep().front());
}

/// Check whether a value is a loop induction variable (i.e., a BlockArgument
/// whose parent operation is a loop construct).
inline bool isLoopInductionVar(Value value) {
  if (auto arg = dyn_cast<BlockArgument>(value)) {
    Operation *parent = arg.getOwner()->getParentOp();
    return parent && isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp,
                         scf::ForallOp, arts::ForOp>(parent);
  }
  return false;
}

/// Collect upper bounds from a while-loop condition for the given iteration
/// argument. Recursively decomposes AND-ed conditions and extracts bounds
/// from less-than / greater-than comparisons.
void collectWhileBounds(Value cond, Value iterArg, SmallVector<Value> &bounds);

/// Compute the loop nesting depth of an operation by counting how many
/// enclosing loop operations (scf::ForOp, scf::ParallelOp, scf::ForallOp,
/// affine::AffineForOp, omp::WsloopOp, arts::ForOp) surround it.
unsigned getLoopDepth(Operation *op);

/// Returns true if the EDT's body contains any loop operations
/// (scf::ForOp, scf::ParallelOp, affine::AffineForOp).
bool containsLoop(EdtOp edt);

/// Return the nearest enclosing loop-like op that contains the given operation.
/// Searches for scf::ForOp, affine::AffineForOp, omp::WsloopOp,
/// scf::ParallelOp, scf::ForallOp, and arts::ForOp.
inline Operation *findNearestLoop(Operation *op) {
  for (Operation *cur = op->getParentOp(); cur; cur = cur->getParentOp()) {
    if (isa<scf::ForOp, affine::AffineForOp, omp::WsloopOp, scf::ParallelOp,
            scf::ForallOp, arts::ForOp>(cur))
      return cur;
  }
  return nullptr;
}

/// Resolve a constant trip count for a loop-like op when all bounds are static.
/// Returns std::nullopt when the trip count cannot be proven statically.
std::optional<int64_t> getStaticTripCount(Operation *loopOp);

/// Compute a capped product of static enclosing loop trip counts.
/// Starts at the parent of `op`, so the operation's own trip count is excluded.
/// Useful for repeated-dispatch cost models that need a lightweight estimate
/// of how many times a region is re-entered.
int64_t getRepeatedParentTripProduct(Operation *op,
                                     int64_t maxProduct = 1 << 20);

/// Returns true when a pointwise arts.for carries scalar libm-heavy floating
/// point work (for example transcendentals or scalar FP helper calls). Fusion
/// passes use this to avoid mixing such loops with arithmetic-only loops,
/// which would otherwise suppress vectorization on the simple stages.
PointwiseLoopComputeClass classifyPointwiseLoopCompute(arts::ForOp loop);

} // namespace arts
} // namespace mlir

#endif // ARTS_UTILS_LOOPUTILS_H
