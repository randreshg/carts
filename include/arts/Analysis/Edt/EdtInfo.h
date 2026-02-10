///==========================================================================///
/// File: EdtInfo.h
///
/// Shared EDT information objects for analysis.
///==========================================================================///

#ifndef ARTS_ANALYSIS_EDT_EDTINFO_H
#define ARTS_ANALYSIS_EDT_EDTINFO_H

#include "arts/ArtsDialect.h"
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
  /// Basic counts
  uint64_t totalOps = 0;
  uint64_t numLoads = 0;
  uint64_t numStores = 0;
  uint64_t numCalls = 0;

  /// Memory traffic estimation
  uint64_t bytesRead = 0;
  uint64_t bytesWritten = 0;

  /// Structural info
  uint64_t maxLoopDepth = 0;
  /// (totalOps - memOps)/max(1, memOps)
  double computeToMemRatio = 0.0;
  /// Program order within function (if known)
  unsigned orderIndex = 0;

  /// Base allocations (original memrefs) read/written by this EDT
  llvm::DenseSet<Value> basesRead, basesWritten;
  llvm::DenseSet<DbAllocOp> dbAllocsRead, dbAllocsWritten;

  /// Detailed per-alloc access metrics
  llvm::DenseMap<DbAllocOp, uint64_t> dbAllocAccessCount, dbAllocAccessBytes;

  /// Distribution-pattern analysis results owned at EDT analysis level.
  /// Maps top-level loops within this EDT to classified compute patterns.
  llvm::DenseMap<Operation *, EdtDistributionPattern> loopDistributionPatterns;
  /// EDT-level summary pattern (unknown if mixed or not classified).
  EdtDistributionPattern dominantDistributionPattern =
      EdtDistributionPattern::unknown;

  bool isReadOnly() const { return !basesRead.empty() && basesWritten.empty(); }
  bool isWriteOnly() const {
    return basesRead.empty() && !basesWritten.empty();
  }
  bool isReadWrite() const {
    return !basesRead.empty() && !basesWritten.empty();
  }
};
} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTINFO_H
