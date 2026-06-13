///==========================================================================///
/// File: SdeToArtsBoundaryAccessPass.cpp
/// sde-accesses-to-arts-deps pass runner (phase orchestration).
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCuTask.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryPasses.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundarySuIterate.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/DenseSet.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

static LogicalResult realizeMovementCarriers(ModuleOp module) {
  return realizeAllToAllMovements(module);
}

static LogicalResult inlineStructuralCarriers(ModuleOp module) {
  SmallVector<sde::SdeSuDistributeOp> distributes;
  module.walk([&](sde::SdeSuDistributeOp op) { distributes.push_back(op); });
  for (sde::SdeSuDistributeOp op : distributes)
    if (failed(inlineSdeSuDistribute(op)))
      return failure();
  return success();
}

static LogicalResult lowerSuIterateCarriers(ModuleOp module) {
  SmallVector<sde::SdeSuIterateOp> iterates;
  module.walk([&](sde::SdeSuIterateOp op) { iterates.push_back(op); });
  DenseSet<Operation *> consumedCuLevelAccessWindows;
  SmallVector<Operation *> consumedRedists;
  for (sde::SdeSuIterateOp op : iterates)
    if (failed(convertSuIterate(op, consumedCuLevelAccessWindows,
                                consumedRedists)))
      return failure();
  for (Operation *op : consumedCuLevelAccessWindows)
    if (op && op->getBlock())
      op->erase();
  for (Operation *redist : consumedRedists)
    if (redist && redist->getBlock())
      redist->erase();
  return success();
}

static LogicalResult lowerCuTaskCarriers(ModuleOp module) {
  SmallVector<sde::SdeCuTaskOp> tasks;
  module.walk([&](sde::SdeCuTaskOp op) { tasks.push_back(op); });
  for (sde::SdeCuTaskOp op : tasks)
    if (failed(convertCuTask(op)))
      return failure();
  return success();
}

static LogicalResult rejectUnconsumedDbAccessWindows(ModuleOp module) {
  bool foundAccessWindow = false;
  module.walk([&](arts::DbAccessWindowOp op) {
    op.emitOpError()
        << "was not consumed during SDE access realization into ARTS "
           "dependencies";
    foundAccessWindow = true;
  });
  return failure(foundAccessWindow);
}

LogicalResult runSdeAccessesToArtsDeps(ModuleOp module) {
  if (failed(realizeMovementCarriers(module)))
    return failure();
  if (failed(inlineStructuralCarriers(module)))
    return failure();
  if (failed(lowerStandaloneCuRegions(module)))
    return failure();
  if (failed(lowerSuIterateCarriers(module)))
    return failure();
  if (failed(lowerCuTaskCarriers(module)))
    return failure();
  return rejectUnconsumedDbAccessWindows(module);
}

} // namespace mlir::carts::arts::boundary
