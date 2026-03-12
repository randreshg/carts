///==========================================================================///
/// File: EdtUtils.h
///
/// Utility class for working with ARTS EDTs (EdtOp).
/// This module consolidates EDT-related utilities including analysis and
/// block argument queries.
///==========================================================================///

#ifndef CARTS_UTILS_EDTUTILS_H
#define CARTS_UTILS_EDTUTILS_H

#include "arts/Dialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Region.h"
#include "mlir/IR/Value.h"

namespace mlir {
class BlockArgument;
}

namespace mlir {
namespace arts {

/// Forward declarations
class EdtOp;
class EpochOp;
class DbAcquireOp;

/// Utility class for working with ARTS EDTs (EdtOp).
class EdtUtils {
public:
  /// Check if a value is invariant within an EDT region.
  /// A value is invariant if it is defined outside the region or not modified
  /// within it.
  static bool isInvariantInEdt(Region &edtRegion, Value value);

  /// Checks if the target operation is reachable from the source operation in
  /// the EDT control flow graph.
  static bool isReachable(Operation *source, Operation *target);

  /// Check if an EDT contains nested EDTs (tasks/parallel/sync).
  static bool hasNestedEdt(EdtOp edt);

  /// Wrap all operations (except terminator) in a block inside an EpochOp.
  /// Returns the created EpochOp, or nullptr if no operations to wrap.
  static EpochOp wrapBodyInEpoch(Block &body, Location loc);

  /// Finds the EdtOp that uses a DbAcquireOp and returns the corresponding
  /// block argument. Returns {EdtOp, BlockArgument} pair, or {nullptr,
  /// BlockArgument()} if not found.
  static std::pair<EdtOp, BlockArgument>
  getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_EDTUTILS_H
