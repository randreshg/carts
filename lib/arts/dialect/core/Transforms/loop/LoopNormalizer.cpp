///==========================================================================///
/// LoopNormalizer.cpp - Shared utilities for loop normalization patterns
///
/// Before:
///   %new = arts.for (...) { ... }   // rebuilt loop with no attrs yet
///
/// After:
///   %new = arts.for (...) { ... }
///          {arts.id = 42, arts.dep_pattern = #arts.dep_pattern<wavefront_2d>}
///==========================================================================///

#include "arts/dialect/core/Transforms/loop/LoopNormalizer.h"
#include "arts/dialect/core/Analysis/db/DbPatternMatchers.h"
#include "arts/utils/OperationAttributes.h"

using namespace mlir;
using namespace mlir::arts;

bool arts::rewriteNormalizedLoop(Operation *source, Operation *target,
                                 MetadataManager &metadataManager) {
  if (!source || !target)
    return false;
  bool rewritten = metadataManager.rewriteLoopMetadata(source, target);
  copyPatternAttrs(source, target);
  return rewritten;
}

std::optional<int64_t> arts::getTriangularOffset(Value lb, Value outerIV) {
  return matchTriangularOffset(lb, outerIV);
}
