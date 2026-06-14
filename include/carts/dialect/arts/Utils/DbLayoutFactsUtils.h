///==========================================================================///
/// File: DbLayoutFactsUtils.h
///
/// Helpers for realizing DB layouts from committed SDE layout facts.
///
/// These utilities do not choose partitioning policy. SDE owns owner
/// dimensions, physical block shape, and halo policy. ARTS utilities may
/// inspect those facts for diagnostics or ARTS object creation, but
/// block-local access rewriting belongs to SDE MU/token lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTSUTILS_H
#define CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTSUTILS_H

#include "carts/dialect/arts/Utils/DbLayoutFacts.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/ValueRange.h"
#include "mlir/Support/LLVM.h"

namespace mlir {
namespace carts::arts {

/// Returns true when an operation carries the minimum SDE-authored physical
/// layout facts needed to create a block DB immediately.
bool hasPhysicalDbLayoutFacts(Operation *op);

/// Build ceil(value / max(divisor, 1)) for index values.
Value ceilDivPositiveIndex(OpBuilder &builder, Location loc, Value value,
                           Value divisor);

/// Resolve explicit owner-dimension and physical-block-shape attrs into a
/// concrete DB layout for an allocation with the provided logical element
/// extents.
FailureOr<DbPhysicalLayoutFacts>
resolvePhysicalDbLayoutFacts(ArrayAttr ownerDimsAttr, ArrayAttr blockShapeAttr,
                             ValueRange elementSizes, OpBuilder &builder,
                             Location loc);

} // namespace carts::arts
} // namespace mlir

#endif // CARTS_DIALECT_ARTS_UTILS_DBLAYOUTFACTSUTILS_H
