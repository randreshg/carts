///==========================================================================///
/// File: EdtInfo.h
///
/// Shared EDT information objects for analysis.
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_EDT_EDTINFO_H
#define ARTS_DIALECT_CORE_ANALYSIS_EDT_EDTINFO_H

#include "arts/Dialect.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace mlir {
namespace arts {

/// Canonical capture contract for one EDT before lowering rewrites mutate the
/// region around nested task creation.
struct EdtCaptureSummary {
  llvm::SmallVector<Value, 8> capturedValues;
  llvm::SmallVector<Value, 8> parameters;
  llvm::SmallVector<Value, 8> constants;
  llvm::SmallVector<Value, 8> dbHandles;
  llvm::SmallVector<Value, 8> dependencies;
};

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
  EdtCaptureSummary captureSummary;
};
} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_EDT_EDTINFO_H
