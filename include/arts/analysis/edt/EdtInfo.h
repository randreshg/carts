///==========================================================================///
/// File: EdtInfo.h
///
/// Shared EDT information objects for analysis.
///==========================================================================///

#ifndef ARTS_ANALYSIS_EDT_EDTINFO_H
#define ARTS_ANALYSIS_EDT_EDTINFO_H

#include "arts/Dialect.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include <cstdint>

namespace mlir {
namespace arts {

/// Base info aggregated for an EDT region.
struct EdtInfo {
  /// Structural info
  uint64_t maxLoopDepth = 0;
  /// Program order within function (if known)
  unsigned orderIndex = 0;

  /// Distribution-pattern analysis results owned at EDT analysis level.
  /// Maps top-level loops within this EDT to classified compute patterns.
  llvm::DenseMap<Operation *, EdtDistributionPattern> loopDistributionPatterns;
  /// EDT-level summary pattern (unknown if mixed or not classified).
  EdtDistributionPattern dominantDistributionPattern =
      EdtDistributionPattern::unknown;
};
} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTINFO_H
