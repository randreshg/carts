///==========================================================================///
/// File: SdeToArtsBoundaryStandaloneCu.h
/// Standalone CU region access realization.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYSTANDALONECU_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYSTANDALONECU_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

LogicalResult realizeStandaloneCuAccesses(sde::SdeCuRegionOp source);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
