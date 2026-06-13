///==========================================================================///
/// File: SdeToArtsBoundaryCommon.h
/// Shared SDE→ARTS boundary cleanup and carrier rejection helpers.
///==========================================================================///

#ifndef CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCOMMON_H
#define CARTS_DIALECT_ARTS_TRANSFORMS_BOUNDARY_SDETOARTSBOUNDARYCOMMON_H

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace carts::arts::boundary {

bool containsDbAccessWindow(sde::SdeCuRegionOp region);

LogicalResult inlineSdeCuRegion(sde::SdeCuRegionOp region);
LogicalResult inlineSdeSuDistribute(sde::SdeSuDistributeOp distribute);
LogicalResult eraseConsumedSdeControlToken(sde::SdeControlTokenOp token);

void eraseDbBackedMemrefDeallocs(ModuleOp module);

LogicalResult rejectUnsupportedSdeCarriers(ModuleOp module);
LogicalResult rejectResidualSdeOps(ModuleOp module);

LogicalResult lowerStandaloneCuRegions(ModuleOp module);

} // namespace carts::arts::boundary
} // namespace mlir

#endif
