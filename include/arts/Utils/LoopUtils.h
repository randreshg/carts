///==========================================================================///
/// File: LoopUtils.h
///
/// Utility functions for querying SCF loop properties.
/// Lightweight inline helpers for worker-loop detection, innermost-loop
/// checks, and bound compatibility — complements the heavier LoopAnalysis
/// framework without requiring an AnalysisManager.
///==========================================================================///

#ifndef CARTS_UTILS_LOOPUTILS_H
#define CARTS_UTILS_LOOPUTILS_H

#include "arts/ArtsDialect.h"
#include "arts/utils/ValueUtils.h"
#include "mlir/Dialect/SCF/IR/SCF.h"

namespace mlir {
namespace arts {

/// Check whether a scf::ForOp is a "worker loop" (i.e., contains at least one
/// arts.edt operation anywhere in its body).
/// Used by epoch-level passes to identify task-spawning loops.
inline bool isWorkerLoop(scf::ForOp loop) {
  bool hasEdt = false;
  loop.walk([&](EdtOp) { hasEdt = true; });
  return hasEdt;
}

/// Check whether a scf::ForOp is the innermost loop (contains no nested
/// scf::ForOp operations). Used by strip-mining and other loop transforms
/// that target leaf loops only.
inline bool isInnermostLoop(scf::ForOp loop) {
  bool hasNested = false;
  loop.getBody()->walk([&](scf::ForOp nested) {
    if (nested != loop)
      hasNested = true;
  });
  return !hasNested;
}

/// Check whether two scf::ForOp loops have compatible bounds (same lower
/// bound, upper bound, and step). Uses ValueUtils::sameValue for comparison.
/// Note: this overload is for scf::ForOp only; arts::ForOp has a separate
/// bounds API and is handled in LoopFusion directly.
inline bool haveCompatibleBounds(scf::ForOp a, scf::ForOp b) {
  return ValueUtils::sameValue(a.getLowerBound(), b.getLowerBound()) &&
         ValueUtils::sameValue(a.getUpperBound(), b.getUpperBound()) &&
         ValueUtils::sameValue(a.getStep(), b.getStep());
}

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_LOOPUTILS_H
