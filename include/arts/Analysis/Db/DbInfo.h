///==========================================================================
/// File: DbInfo.h
/// Shared DB information object for analysis.
///==========================================================================

#ifndef ARTS_ANALYSIS_DB_DBINFO_H
#define ARTS_ANALYSIS_DB_DBINFO_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace arts {

/// DbInfo aggregates facts and hints about a datablock.
struct DbInfo {
  /// Basic facts
  bool isAlloc = false;
  unsigned inCount = 0;
  unsigned outCount = 0;

  /// Slice information (value-based)
  SmallVector<Value, 4> offsets;
  SmallVector<Value, 4> sizes;
  /// Constant fallbacks: INT64_MIN if unknown
  SmallVector<int64_t, 4> constOffsets;
  SmallVector<int64_t, 4> constSizes;
  /// Pinned leading indices if any (values)
  SmallVector<Value, 4> pinned;
  /// Estimated byte size for the described slice or allocation.
  /// 0 if unknown.
  uint64_t estimatedBytes = 0;

  /// Join two DbInfo objects, preferably preserving the left-hand side
  /// value-based slices and merging basic counts.
  static DbInfo join(const DbInfo &a, const DbInfo &b) {
    DbInfo r;
    r.isAlloc = a.isAlloc || b.isAlloc;
    r.inCount = a.inCount + b.inCount;
    r.outCount = a.outCount + b.outCount;
    /// Prefer lhs offsets/sizes if present, otherwise rhs
    r.offsets = !a.offsets.empty() ? a.offsets : b.offsets;
    r.sizes = !a.sizes.empty() ? a.sizes : b.sizes;
    return r;
  }

  bool operator==(const DbInfo &o) const {
    return isAlloc == o.isAlloc && inCount == o.inCount &&
           outCount == o.outCount && offsets == o.offsets && sizes == o.sizes;
  }

  /// Print a compact representation for debugging.
  void print(raw_ostream &os) const {
    os << "DbInfo(alloc=" << (isAlloc ? 1 : 0) << ", in=" << inCount
       << ", out=" << outCount << ")";
  }
};

/// Acquire-level info: extends DbInfo with lifetime indices and ops.
struct DbAcquireInfo : public DbInfo {
  unsigned beginIndex = 0;
  /// inclusive order index of matching release
  unsigned endIndex = 0;
  DbAcquireOp acquire;
  DbReleaseOp release;
};

/// Allocation-level summary: extends DbInfo with aggregated metrics.
struct DbAllocInfo : public DbInfo {
  unsigned allocIndex = 0;
  /// last release order
  unsigned endIndex = 0;
  unsigned numAcquires = 0;
  unsigned numReleases = 0;
  /// 0 if unknown
  uint64_t staticBytes = 0;
  SmallVector<DbAcquireInfo, 8> intervals;
  bool isLongLived = false;
  bool maybeEscaping = false;
  unsigned maxLoopDepth = 0;
  unsigned criticalSpan = 0;
  unsigned criticalPath = 0;
  uint64_t totalAccessBytes = 0;
  /// Reuse info (graph coloring and candidate matches)
  unsigned reuseColor = 0;
  SmallVector<DbAllocOp, 8> reuseMatches;
};

/// Release-level info: extends DbInfo with release position and op.
struct DbReleaseInfo : public DbInfo {
  unsigned releaseIndex = 0;
  DbReleaseOp release;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBINFO_H
