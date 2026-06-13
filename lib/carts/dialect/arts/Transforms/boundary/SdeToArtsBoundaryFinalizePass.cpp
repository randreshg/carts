///==========================================================================///
/// File: SdeToArtsBoundaryFinalizePass.cpp
/// finalize-sde-to-arts pass runner.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op);
LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op);

LogicalResult runFinalizeSdeToArts(ModuleOp module) {
  SmallVector<sde::SdeResourceQueryOp> resourceQueries;
  module.walk([&](sde::SdeResourceQueryOp op) { resourceQueries.push_back(op); });
  for (sde::SdeResourceQueryOp op : resourceQueries)
    if (failed(lowerSdeResourceQuery(op)))
      return failure();

  SmallVector<sde::SdeSuBarrierOp> controlBarriers;
  module.walk([&](sde::SdeSuBarrierOp op) { controlBarriers.push_back(op); });
  for (sde::SdeSuBarrierOp op : controlBarriers)
    if (failed(lowerSdeControlBarrier(op)))
      return failure();

  SmallVector<sde::SdeSuDistributeOp> distributes;
  module.walk([&](sde::SdeSuDistributeOp op) { distributes.push_back(op); });
  for (sde::SdeSuDistributeOp op : distributes)
    if (failed(inlineSdeSuDistribute(op)))
      return failure();

  SmallVector<sde::SdeControlTokenOp> controlTokens;
  module.walk([&](sde::SdeControlTokenOp op) { controlTokens.push_back(op); });
  for (sde::SdeControlTokenOp op : controlTokens)
    if (failed(eraseConsumedSdeControlToken(op)))
      return failure();

  SmallVector<sde::SdeMuTokenOp> tokens;
  module.walk([&](sde::SdeMuTokenOp op) { tokens.push_back(op); });
  for (sde::SdeMuTokenOp token : tokens) {
    if (!token.getToken().use_empty()) {
      token.emitOpError()
          << "survived direct SDE-to-ARTS lowering with live users";
      return failure();
    }
    token.erase();
  }

  SmallVector<sde::SdeCuRegionOp> regions;
  module.walk([&](sde::SdeCuRegionOp op) { regions.push_back(op); });
  for (sde::SdeCuRegionOp region : regions)
    if (failed(inlineSdeCuRegion(region)))
      return failure();

  eraseDbBackedMemrefDeallocs(module);

  bool foundAccessWindow = false;
  module.walk([&](arts::DbAccessWindowOp op) {
    op.emitOpError()
        << "remains after SDE access realization into ARTS dependencies";
    foundAccessWindow = true;
  });
  if (foundAccessWindow)
    return failure();

  return rejectResidualSdeOps(module);
}

} // namespace mlir::carts::arts::boundary
