///==========================================================================///
/// File: HeuristicUtils.cpp
///
/// Shared heuristic building blocks for H1 (Partitioning) and H2
/// (Distribution) heuristic families.
///==========================================================================///

#include "arts/Analysis/HeuristicUtils.h"
#include "arts/Analysis/Loop/LoopNode.h"
#include "arts/ArtsDialect.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "arts/Utils/OperationAttributes.h"

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
/// Loop Classification Helpers
///===----------------------------------------------------------------------===///

bool mlir::arts::isSequentialLoop(ForOp forOp, LoopNode *loopNode) {
  /// Check LoopNode fields first (already resolved metadata).
  if (loopNode) {
    if (loopNode->hasInterIterationDeps && *loopNode->hasInterIterationDeps)
      return true;
    if (loopNode->parallelClassification &&
        *loopNode->parallelClassification ==
            LoopMetadata::ParallelClassification::Sequential)
      return true;
  }

  /// Fall back to on-operation LoopMetadataAttr.
  if (auto loopAttr = forOp->getAttrOfType<LoopMetadataAttr>(
          AttrNames::LoopMetadata::Name)) {
    if (auto depsAttr = loopAttr.getHasInterIterationDeps())
      if (depsAttr.getValue())
        return true;

    if (auto classAttr = loopAttr.getParallelClassification()) {
      if (classAttr.getInt() ==
          static_cast<int64_t>(
              LoopMetadata::ParallelClassification::Sequential))
        return true;
    }
  }

  return false;
}
