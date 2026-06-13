///==========================================================================///
/// File: SdeToArtsBoundarySuIterate.h
/// SDE SU iterate → ARTS EDT lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYSUITERATE_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYSUITERATE_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseSet.h"

namespace mlir {
namespace carts::arts::boundary {

LogicalResult convertSuIterate(
    sde::SdeSuIterateOp source,
    DenseSet<Operation *> &consumedCuLevelAccessWindows,
    SmallVectorImpl<Operation *> &consumedRedists);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
