///==========================================================================///
/// File: EdtUtils.h
///
/// Utility class for working with ARTS EDTs (EdtOp).
/// This module consolidates EDT-related utilities including analysis and
/// block argument queries.
///==========================================================================///

#ifndef CARTS_UTILS_EDTUTILS_H
#define CARTS_UTILS_EDTUTILS_H

#include "arts/ArtsDialect.h"
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
class DbAcquireOp;

///===----------------------------------------------------------------------===///
/// EDT Utilities
///===----------------------------------------------------------------------===///
/// Utility class for working with ARTS EDTs (EdtOp).
class EdtUtils {
public:
  ///===----------------------------------------------------------------------===///
  /// EDT Analysis Utilities
  ///===----------------------------------------------------------------------===///
  /// Functions for analyzing EDT regions and operations

  /// Check if a value is invariant within an EDT region.
  /// A value is invariant if it is defined outside the region or not modified
  /// within it.
  static bool isInvariantInEdt(Region &edtRegion, Value value);

  /// Checks if the target operation is reachable from the source operation in
  /// the EDT control flow graph.
  static bool isReachable(Operation *source, Operation *target);

  ///===----------------------------------------------------------------------===///
  /// EDT Block Argument Utilities
  ///===----------------------------------------------------------------------===///
  /// Functions for querying EDT block arguments

  /// Finds the EdtOp that uses a DbAcquireOp and returns the corresponding block
  /// argument.
  /// Returns {EdtOp, BlockArgument} pair, or {nullptr, BlockArgument()} if not found.
  static std::pair<EdtOp, BlockArgument>
  getEdtBlockArgumentForAcquire(DbAcquireOp acquireOp);
};

} // namespace arts
} // namespace mlir

#endif // CARTS_UTILS_EDTUTILS_H
