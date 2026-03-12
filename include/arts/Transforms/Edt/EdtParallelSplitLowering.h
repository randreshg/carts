///==========================================================================///
/// File: EdtParallelSplitLowering.h
///
/// Split-analysis and continuation EDT lowering helpers used by ForLowering.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_EDT_EDTPARALLELSPLITLOWERING_H
#define ARTS_TRANSFORMS_EDT_EDTPARALLELSPLITLOWERING_H

#include "arts/ArtsDialect.h"
#include "arts/codegen/Codegen.h"
#include "llvm/ADT/SetVector.h"

namespace mlir {
namespace arts {

struct ParallelRegionSplitAnalysis {
  SmallVector<Operation *, 8> opsBeforeFor;
  SmallVector<ForOp, 4> forOps;
  SmallVector<Operation *, 8> opsAfterFor;

  SetVector<BlockArgument> depsUsedByFor;
  SetVector<BlockArgument> depsUsedAfterFor;

  DenseMap<unsigned, Value> argIndexToDep;

  bool hasWorkBefore() const { return !opsBeforeFor.empty(); }
  bool hasWorkAfter() const { return !opsAfterFor.empty(); }
  bool needsSplit() const { return !forOps.empty(); }

  static ParallelRegionSplitAnalysis analyze(EdtOp parallelEdt);
  void analyzeDependenciesForSplit(EdtOp parallelEdt);
};

/// Create continuation parallel EDT for post-for operations.
EdtOp createContinuationParallel(ArtsCodegen &AC, EdtOp originalParallel,
                                 ParallelRegionSplitAnalysis &analysis,
                                 Location loc);

/// Cleanup original parallel region after split/lowering rewrites.
void cleanupOriginalParallel(EdtOp originalParallel,
                             ParallelRegionSplitAnalysis &analysis,
                             bool hasPreFor);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_EDT_EDTPARALLELSPLITLOWERING_H

