///==========================================================================///
/// File: SdeToArtsBoundaryCommon.cpp
/// SDE→ARTS boundary lowering unit.
///==========================================================================///

#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryCommon.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryStandaloneCu.h"
#include "carts/dialect/arts/Transforms/boundary/SdeToArtsBoundaryTypes.h"
#include "carts/dialect/arts/Utils/DbBackedMemrefUtils.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/LaunchPolicyUtils.h"
#include "carts/dialect/arts/Utils/MovementLoweringUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/MuAccessWindow.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/Utils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include <algorithm>
#include <functional>
#include <limits>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {
bool containsDbAccessWindow(sde::SdeCuRegionOp source) {
  bool found = false;
  source.getBody().walk([&](arts::DbAccessWindowOp) {
    found = true;
    return WalkResult::interrupt();
  });
  return found;
}

LogicalResult inlineSdeSuDistribute(sde::SdeSuDistributeOp op) {
  if (op.getBody().empty() || op.getBody().front().getNumArguments() != 0)
    return op.emitOpError()
           << "has non-empty region arguments during SDE-to-ARTS cleanup";

  Block &body = op.getBody().front();
  if (!body.empty())
    if (auto yield = dyn_cast<sde::SdeYieldOp>(&body.back()))
      yield.erase();

  op->getBlock()->getOperations().splice(Block::iterator(op.getOperation()),
                                         body.getOperations());
  op.erase();
  return success();
}

LogicalResult eraseConsumedSdeControlToken(sde::SdeControlTokenOp op) {
  if (!op.getToken().use_empty())
    return op.emitOpError()
           << "survived SDE-to-ARTS boundary conversion with live users";
  op.erase();
  return success();
}

LogicalResult inlineSdeCuRegion(sde::SdeCuRegionOp op) {
  if (op.getBody().empty())
    return op.emitOpError() << "has no body during SDE-to-ARTS cleanup";
  Block &body = op.getBody().front();
  if (body.getNumArguments() != op.getIterArgs().size())
    return op.emitOpError()
           << "has mismatched body arguments during SDE-to-ARTS cleanup";
  for (auto [arg, iterArg] : llvm::zip(body.getArguments(), op.getIterArgs()))
    arg.replaceAllUsesWith(iterArg);

  SmallVector<Value> yielded;
  if (!body.empty())
    if (auto yield = dyn_cast<sde::SdeYieldOp>(&body.back())) {
      yielded.append(yield.getValues().begin(), yield.getValues().end());
      yield.erase();
    }
  if (yielded.size() != op.getNumResults())
    return op.emitOpError()
           << "has mismatched yield/result count during SDE-to-ARTS cleanup";

  OpBuilder builder(op);
  builder.setInsertionPoint(op);
  op->getBlock()->getOperations().splice(Block::iterator(op.getOperation()),
                                         body.getOperations());
  for (auto [result, replacement] : llvm::zip(op.getResults(), yielded))
    result.replaceAllUsesWith(replacement);
  op.erase();
  return success();
}
void eraseDbBackedMemrefDeallocs(ModuleOp module) {
  SmallVector<memref::DeallocOp> deallocs;
  module.walk([&](memref::DeallocOp dealloc) {
    if (arts::DbUtils::getUnderlyingDbAlloc(dealloc.getMemref()))
      deallocs.push_back(dealloc);
  });
  for (memref::DeallocOp dealloc : deallocs)
    dealloc.erase();
}

LogicalResult rejectUnsupportedSdeCarriers(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (isa<sde::SdeCuWorkOp>(op)) {
      op->emitError()
          << "direct SDE-to-ARTS lowering for this SDE carrier is not yet "
             "implemented; add a real ARTS realization";
      found = true;
    }
  });
  return failure(found);
}

LogicalResult rejectResidualSdeOps(ModuleOp module) {
  bool found = false;
  module.walk([&](Operation *op) {
    if (!op->getDialect() || op->getDialect()->getNamespace() != "sde")
      return;
    op->emitError() << "SDE operation '" << op->getName()
                    << "' remains after direct SDE-to-ARTS conversion";
    found = true;
  });
  return failure(found);
}

LogicalResult lowerStandaloneCuRegions(ModuleOp module) {
  auto collectStandalone = [&](SmallVectorImpl<sde::SdeCuRegionOp> &out) {
    out.clear();
    module.walk([&](sde::SdeCuRegionOp op) {
      if (!op->getParentOfType<sde::SdeSuIterateOp>())
        out.push_back(op);
    });
  };

  SmallVector<sde::SdeCuRegionOp, 8> regions;
  collectStandalone(regions);
  for (sde::SdeCuRegionOp op : regions)
    if (op && op->getBlock() && !containsDbAccessWindow(op))
      if (failed(inlineSdeCuRegion(op)))
        return failure();

  collectStandalone(regions);
  for (sde::SdeCuRegionOp op : regions)
    if (failed(realizeStandaloneCuAccesses(op)))
      return failure();

  collectStandalone(regions);
  for (sde::SdeCuRegionOp op : regions)
    if (failed(inlineSdeCuRegion(op)))
      return failure();
  return success();
}
} // namespace mlir::carts::arts::boundary
