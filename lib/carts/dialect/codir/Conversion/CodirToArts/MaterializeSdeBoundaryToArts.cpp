///==========================================================================///
/// File: MaterializeSdeBoundaryToArts.cpp
///
/// Explicitly materializes residual SDE storage/control boundary artifacts
/// before CODIR-to-ARTS runs.
///==========================================================================///
#include "ArtsMaterializationUtils.h"
#include "carts/dialect/codir/Conversion/Passes.h"

namespace mlir::carts::codir {
#define GEN_PASS_DEF_MATERIALIZESDEBOUNDARYTOARTS
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir

using namespace mlir;
using namespace mlir::carts;

namespace {

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
