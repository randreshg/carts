///==========================================================================///
/// File: DbVersionedRewriter.h
///
/// Versioned partitioning rewriter for DbCopy/DbSync.
/// Disabled currently; kept for future enablement.
///==========================================================================///

#ifndef ARTS_TRANSFORMS_DATABLOCK_VERSIONED_DBVERSIONEDREWRITER_H
#define ARTS_TRANSFORMS_DATABLOCK_VERSIONED_DBVERSIONEDREWRITER_H

#include "arts/ArtsDialect.h"
#include "mlir/IR/Value.h"

namespace mlir {
namespace arts {

/// Per-acquire information needed for versioned partitioning.
struct VersionedAcquireInfo {
  DbAcquireOp acquire;
  Value chunkIndex;
  Value chunkSize;
};

/// Rewriter for versioned partitioning (DbCopy/DbSync).
class DbVersionedRewriter {
public:
  /// Disabled: do not call until versioned partitioning is re-enabled.
  static void apply(DbAllocOp allocOp,
                    ArrayRef<VersionedAcquireInfo> versionedAcquires,
                    ArrayRef<Value> oldElementSizes);
};

} // namespace arts
} // namespace mlir

#endif /// ARTS_TRANSFORMS_DATABLOCK_VERSIONED_DBVERSIONEDREWRITER_H
