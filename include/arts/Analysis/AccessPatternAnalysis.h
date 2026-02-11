///==========================================================================///
/// File: AccessPatternAnalysis.h
///
/// Shared utilities for analyzing loop-relative memory access bounds.
///==========================================================================///

#ifndef ARTS_ANALYSIS_ACCESSPATTERNANALYSIS_H
#define ARTS_ANALYSIS_ACCESSPATTERNANALYSIS_H

#include "mlir/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>
#include <optional>

namespace mlir {
namespace arts {

/// Captures one memory access index chain and db_ref prefix rank information.
struct AccessIndexInfo {
  llvm::SmallVector<Value, 8> indexChain;
  unsigned dbRefPrefix = 0;
};

/// Loop-relative access bounds summary.
struct AccessBoundsResult {
  int64_t minOffset = 0;
  int64_t maxOffset = 0;
  int64_t centerOffset = 0;
  bool isStencil = false;
  bool valid = false;
  bool hasVariableOffset = false;
};

/// Normalize stencil bounds around zero for uniformly shifted stencils.
void normalizeAccessBounds(AccessBoundsResult &bounds);

/// Analyze access bounds from index chains relative to loopIV + blockBase.
AccessBoundsResult analyzeAccessBoundsFromIndices(
    llvm::ArrayRef<AccessIndexInfo> accesses, Value loopIV, Value blockBase,
    std::optional<unsigned> partitionDim = std::nullopt);

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_ACCESSPATTERNANALYSIS_H
