///==========================================================================///
/// File: AcquireRewritePlanning.h
///
/// Strategy-aware planning for per-task DbAcquire rewriting.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_EDT_ACQUIREREWRITEPLANNING_H
#define ARTS_TRANSFORMS_EDT_ACQUIREREWRITEPLANNING_H

#include "arts/analysis/DistributionHeuristics.h"
#include "arts/transforms/edt/EdtRewriter.h"
#include <optional>

namespace mlir {
namespace arts {

class AnalysisManager;

/// Inputs required to plan one rewritten task acquire.
struct AcquireRewritePlanningInput {
  ArtsCodegen *AC = nullptr;
  AnalysisManager *analysisManager = nullptr;
  Location loc;

  ForOp loopOp;
  DbAcquireOp parentAcquire;
  Value rootGuid;
  Value rootPtr;

  DistributionKind distributionKind = DistributionKind::Flat;
  std::optional<EdtDistributionPattern> distributionPattern;
  std::optional<Tiling2DWorkerGrid> tiling2DGrid;

  Value acquireOffset;
  Value acquireSize;
  Value acquireHintSize;
  Value step;
  bool stepIsUnit = true;
};

/// Planned rewrite result consumed by ForLowering.
struct AcquireRewritePlan {
  AcquireRewriteInput rewriteInput;
  bool useStencilRewriter = false;
};

/// Build rewrite inputs and choose block-vs-stencil rewriter flavor.
AcquireRewritePlan planAcquireRewrite(AcquireRewritePlanningInput input);

} // namespace arts
} // namespace mlir

#endif // ARTS_TRANSFORMS_EDT_ACQUIREREWRITEPLANNING_H
