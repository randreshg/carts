///==========================================================================///
/// File: MaterializeSdeBoundaryToArts.cpp
///
/// Explicitly materializes residual SDE storage/control boundary artifacts
/// before CODIR-to-ARTS runs.
///==========================================================================///
#include "CodirToArtsDbBackedMemref.h"
#include "carts/dialect/codir/Conversion/Passes.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_MATERIALIZESDEBOUNDARYTOARTS
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

static LogicalResult lowerMuData(sde::SdeMuDataOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getHandle().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref handle before CODIR-to-ARTS materialization";
  if (memrefType.getNumDynamicDims() != 0)
    return op.emitOpError()
           << "has dynamic dimensions but carries no dynamic size operands";

  OpBuilder builder(op);
  Value replacement;
  if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                  ValueRange{}, replacement)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getHandle().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static LogicalResult lowerMuAlloc(sde::SdeMuAllocOp op) {
  auto memrefType = dyn_cast<MemRefType>(op.getMemref().getType());
  if (!memrefType)
    return op.emitOpError()
           << "expects a memref result before CODIR-to-ARTS materialization";

  OpBuilder builder(op);
  FailureOr<sde::SdeSuIterateOp> planSource = selectMuAllocWritePlan(op);
  if (failed(planSource))
    return failure();

  Value replacement;
  if (failed(createDbBackedMemref(builder, op.getLoc(), memrefType,
                                  op.getDynamicSizes(), replacement,
                                  *planSource)))
    return failure();
  if (replacement.getType() != memrefType)
    replacement =
        memref::CastOp::create(builder, op.getLoc(), memrefType, replacement);

  op.getMemref().replaceAllUsesWith(replacement);
  op.erase();
  return success();
}

static LogicalResult lowerSdeResourceQuery(sde::SdeResourceQueryOp op) {
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

static LogicalResult lowerSdeControlBarrier(sde::SdeSuBarrierOp op) {
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

static LogicalResult eraseConsumedSdeControlToken(sde::SdeControlTokenOp op) {
  if (!op.getToken().use_empty())
    return op.emitOpError()
           << "survived CODIR-to-ARTS materialization with live users; "
              "control tokens must be consumed by SDE barriers before the "
              "ARTS boundary";
  op.erase();
  return success();
}

struct MaterializeSdeBoundaryToArtsPass
    : public codir::impl::MaterializeSdeBoundaryToArtsBase<
          MaterializeSdeBoundaryToArtsPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();

    SmallVector<sde::SdeMuDataOp> muDatas;
    module.walk([&](sde::SdeMuDataOp op) { muDatas.push_back(op); });
    for (sde::SdeMuDataOp op : muDatas) {
      if (failed(lowerMuData(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeMuAllocOp> muAllocs;
    module.walk([&](sde::SdeMuAllocOp op) { muAllocs.push_back(op); });
    for (sde::SdeMuAllocOp op : muAllocs) {
      if (failed(lowerMuAlloc(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeResourceQueryOp> resourceQueries;
    module.walk(
        [&](sde::SdeResourceQueryOp op) { resourceQueries.push_back(op); });
    for (sde::SdeResourceQueryOp op : resourceQueries) {
      if (failed(lowerSdeResourceQuery(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeSuBarrierOp> controlBarriers;
    module.walk([&](sde::SdeSuBarrierOp op) { controlBarriers.push_back(op); });
    for (sde::SdeSuBarrierOp op : controlBarriers) {
      if (failed(lowerSdeControlBarrier(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeControlTokenOp> controlTokens;
    module.walk(
        [&](sde::SdeControlTokenOp op) { controlTokens.push_back(op); });
    for (sde::SdeControlTokenOp op : controlTokens) {
      if (failed(eraseConsumedSdeControlToken(op))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeMuTokenOp> tokens;
    module.walk([&](sde::SdeMuTokenOp op) { tokens.push_back(op); });
    for (sde::SdeMuTokenOp token : tokens) {
      if (!token.getToken().use_empty()) {
        token.emitOpError()
            << "survived SDE boundary materialization; run "
               "`convert-sde-to-codir` before "
               "`materialize-sde-boundary-to-arts` so SDE codelets become "
               "CODIR codelets before ARTS lowering";
        signalPassFailure();
        return;
      }
      token.erase();
    }
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::carts::codir::createMaterializeSdeBoundaryToArtsPass() {
  return std::make_unique<MaterializeSdeBoundaryToArtsPass>();
}
