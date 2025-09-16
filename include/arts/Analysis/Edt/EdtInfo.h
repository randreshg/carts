///==========================================================================
/// File: EdtInfo.h
/// Shared EDT information objects for analysis.
///==========================================================================

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
  llvm::DenseSet<Value> basesRead;
  llvm::DenseSet<Value> basesWritten;

  /// When DbGraph is available: DbAlloc ops (read/written)
  llvm::DenseSet<DbAllocOp> dbAllocsRead;
  llvm::DenseSet<DbAllocOp> dbAllocsWritten;

  /// Detailed per-alloc access metrics
  /// (loads+stores)
  llvm::DenseMap<DbAllocOp, uint64_t> dbAllocAccessCount;
  /// Estimated bytes
  llvm::DenseMap<DbAllocOp, uint64_t> dbAllocAccessBytes;

  bool isReadOnly() const { return !basesRead.empty() && basesWritten.empty(); }
  bool isWriteOnly() const {
    return basesRead.empty() && !basesWritten.empty();
  }
  bool isReadWrite() const {
    return !basesRead.empty() && !basesWritten.empty();
  }

  static EdtInfo join(const EdtInfo &a, const EdtInfo &b) {
    EdtInfo r;
    // Sum basic counts
    r.totalOps = a.totalOps + b.totalOps;
    r.numLoads = a.numLoads + b.numLoads;
    r.numStores = a.numStores + b.numStores;
    r.numCalls = a.numCalls + b.numCalls;
    /// Sum traffic
    r.bytesRead = a.bytesRead + b.bytesRead;
    r.bytesWritten = a.bytesWritten + b.bytesWritten;
    /// Max structural depth
    r.maxLoopDepth = std::max<uint64_t>(a.maxLoopDepth, b.maxLoopDepth);
    /// Preserve earliest order index
    r.orderIndex = std::min<unsigned>(a.orderIndex, b.orderIndex);
    /// Union sets
    r.basesRead = a.basesRead;
    r.basesRead.insert(b.basesRead.begin(), b.basesRead.end());
    r.basesWritten = a.basesWritten;
    r.basesWritten.insert(b.basesWritten.begin(), b.basesWritten.end());
    r.dbAllocsRead = a.dbAllocsRead;
    r.dbAllocsRead.insert(b.dbAllocsRead.begin(), b.dbAllocsRead.end());
    r.dbAllocsWritten = a.dbAllocsWritten;
    r.dbAllocsWritten.insert(b.dbAllocsWritten.begin(),
                             b.dbAllocsWritten.end());
    /// Merge maps by summing values
    r.dbAllocAccessCount = a.dbAllocAccessCount;
    for (auto &kv : b.dbAllocAccessCount)
      r.dbAllocAccessCount[kv.first] += kv.second;
    r.dbAllocAccessBytes = a.dbAllocAccessBytes;
    for (auto &kv : b.dbAllocAccessBytes)
      r.dbAllocAccessBytes[kv.first] += kv.second;
    /// Recompute ratio
    const uint64_t memOps = r.numLoads + r.numStores;
    r.computeToMemRatio =
        memOps ? double(r.totalOps - memOps) / double(memOps) : 0.0;
    return r;
  }
};

/// Pairwise affinity between two EDTs for placement decisions.
struct EdtPairAffinity {
  double dataOverlap = 0.0;     /// Jaccard overlap on base sets
  double hazardScore = 0.0;     /// fraction of overlapping bases with writers
  bool mayConflict = false;     /// true if hazardScore > 0
  double reuseProximity = 0.0;  /// higher when edts are closer in program order
  double localityScore = 0.0;   /// dataOverlap * reuseProximity
  double concurrencyRisk = 0.0; /// hazardScore * dataOverlap
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_EDT_EDTINFO_H
