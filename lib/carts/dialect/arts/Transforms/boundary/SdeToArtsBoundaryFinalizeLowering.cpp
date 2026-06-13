///==========================================================================///
/// File: SdeToArtsBoundaryFinalizeLowering.cpp
/// SDE control/resource query lowering for finalize-sde-to-arts.
///==========================================================================///

#include "carts/dialect/arts/Utils/RuntimeOpUtils.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Support/LogicalResult.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace mlir::carts::arts::boundary {

LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op) {
  OpBuilder builder(op);
  switch (op.getKind()) {
  case sde::SdeResourceQueryKind::logicalWorkers: {
    auto runtimeQuery = arts::RuntimeQueryOp::create(
        builder, op.getLoc(), arts::RuntimeQueryKind::totalWorkers);
    Value asIndex = arith::IndexCastOp::create(
        builder, op.getLoc(), builder.getIndexType(), runtimeQuery.getResult());
    op.getResult().replaceAllUsesWith(asIndex);
    op.erase();
    return success();
  }
  }
  return op.emitOpError() << "unsupported SDE resource query kind";
}

LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op) {
  OpBuilder builder(op);
  auto reasonAttr = op.getBarrierReasonAttr();
  arts::BarrierOp::create(
      builder, op.getLoc(),
      reasonAttr ? arts::ArtsBarrierReasonAttr::get(
                       op.getContext(), static_cast<arts::ArtsBarrierReason>(
                                            reasonAttr.getValue()))
                 : arts::ArtsBarrierReasonAttr{});
  op.erase();
  return success();
}

} // namespace mlir::carts::arts::boundary
