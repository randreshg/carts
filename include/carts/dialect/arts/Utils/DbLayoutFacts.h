///==========================================================================///
/// File: DbLayoutFacts.h
///
/// Physical DB layout data projected from committed SDE layout facts.
///
/// SDE owns memref/access analysis and chooses the physical owner
/// dimensions, block shape, and task slice facts. ARTS may use this small
/// value type for object creation and diagnostics, but block-local
/// access rewriting belongs to SDE token-local memref lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTS_H
#define CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTS_H

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/PartitionPredicates.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "llvm/ADT/SmallVector.h"

namespace mlir {
namespace carts::arts {

/// Concrete physical layout derived from SDE layout facts.
struct DbPhysicalLayoutFacts {
  PartitionMode mode = PartitionMode::fine_grained;

  /// N-D block sizes, one per partitioned owner dimension.
  SmallVector<Value> blockSizes;
  /// Original memref dimension index for each partitioned owner dim.
  SmallVector<unsigned> partitionedDims;

  /// Allocation shape.  `outerSizes` are DB coordinates; `innerSizes` are the
  /// element memref shape stored inside each DB entry.
  SmallVector<Value> outerSizes;
  SmallVector<Value> innerSizes;

  bool isValid() const {
    if (arts::usesBlockLayout(mode))
      return !blockSizes.empty();
    return true;
  }

  explicit DbPhysicalLayoutFacts(PartitionMode m) : mode(m) {}
};

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTS_H
