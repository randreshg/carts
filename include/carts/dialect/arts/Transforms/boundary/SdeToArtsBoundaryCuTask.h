///==========================================================================///
/// File: SdeToArtsBoundaryCuTask.h
/// SDE CU task → ARTS EDT lowering.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCUTASK_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCUTASK_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

LogicalResult convertCuTask(sde::SdeCuTaskOp source);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
