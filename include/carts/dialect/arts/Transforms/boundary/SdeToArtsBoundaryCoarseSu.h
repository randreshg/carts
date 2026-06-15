///==========================================================================///
/// File: SdeToArtsBoundaryCoarseSu.h
/// Coarse SDE SU iterate access realization.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCOARSESU_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCOARSESU_H

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

LogicalResult convertCoarseSuIterate(sde::SdeSuIterateOp source,
                                     SmallVectorImpl<CoarseSuDependency> &deps);
LogicalResult tryConvertCoarseSuIterate(sde::SdeSuIterateOp source);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
