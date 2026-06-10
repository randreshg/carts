///==========================================================================///
/// File: CodirToArtsSdeBoundary.h
///
/// Internal SDE boundary lowering used by the single CODIR-to-ARTS pass.
///==========================================================================///

#ifndef CARTS_CODIR_TO_ARTS_SDE_BOUNDARY_H
#define CARTS_CODIR_TO_ARTS_SDE_BOUNDARY_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir::carts {

LogicalResult lowerSdeBoundaryToArts(ModuleOp module);

} // namespace mlir::carts

#endif // CARTS_CODIR_TO_ARTS_SDE_BOUNDARY_H
