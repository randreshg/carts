///==========================================================================///
/// File: SdeToCodir.cpp
///
/// Converts SDE facts into isolated CODIR codelets.
///==========================================================================///
#include "carts/dialect/codir/Conversion/Passes.h"
#include "carts/dialect/codir/Utils/CodirConversionUtils.h"
namespace mlir::carts::codir {
#define GEN_PASS_DEF_CONVERTSDETOCODIR
#include "carts/dialect/codir/Conversion/Passes.h.inc"
} // namespace mlir::carts::codir
namespace {
static LogicalResult materializeSdeAtomicsToCodir(ModuleOp module) {
  SmallVector<sde::SdeCuAtomicOp> atomics;
  module.walk([&](sde::SdeCuAtomicOp op) { atomics.push_back(op); });
  for (sde::SdeCuAtomicOp atomic : atomics) {
    if (atomic.getReductionKind() != sde::SdeReductionKind::add)
      return atomic.emitOpError()
             << "cannot materialize non-add SDE atomic at the CODIR boundary";
    OpBuilder builder(atomic);
    codir::AtomicAddOp::create(builder, atomic.getLoc(), atomic.getAddr(),
                               atomic.getValue());
    atomic.erase();
  }
  return success();
}

struct ConvertSdeToCodirPass
    : public codir::impl::ConvertSdeToCodirBase<ConvertSdeToCodirPass> {
  void runOnOperation() override {
    SmallVector<sde::SdeCuWorkOp> codelets;
    getOperation().walk(
        [&](sde::SdeCuWorkOp codelet) { codelets.push_back(codelet); });

    for (sde::SdeCuWorkOp sdeCodelet : codelets) {
      SmallVector<Value> deps;
      SmallVector<Attribute> depModes;
      SmallVector<Attribute> depStorageViews;
      SmallVector<SlicedTokenLocalIndexRewrite> localIndexRewrites;
      OpBuilder builder(sdeCodelet);
      if (failed(materializeCodirDeps(sdeCodelet, builder, deps, depModes,
                                      depStorageViews, localIndexRewrites))) {
        signalPassFailure();
        return;
      }

      SmallVector<Value> params(sdeCodelet.getCaptures().begin(),
                                sdeCodelet.getCaptures().end());
      unsigned originalParamCount = params.size();
      appendSlicedTokenOffsetParams(localIndexRewrites, params);
      appendDynamicCodirDepSliceParams(deps, params);
      auto codirCodelet = createCodirCodelet(
          builder, sdeCodelet.getLoc(), builder.getArrayAttr(depModes),
          builder.getArrayAttr(depStorageViews), deps, params);
      codirCodelet.getBody().takeBody(sdeCodelet.getBody());
      Block &body = codirCodelet.getBody().front();
      for (unsigned idx = originalParamCount, e = params.size(); idx < e; ++idx)
        body.addArgument(params[idx].getType(), sdeCodelet.getLoc());
      if (failed(rewriteTokenLocalAccesses(codirCodelet, localIndexRewrites))) {
        signalPassFailure();
        return;
      }
      replaceSdeYieldWithCodirYield(codirCodelet);
      sdeCodelet.erase();
    }

    SmallVector<sde::SdeCuTaskOp> tasks;
    getOperation().walk([&](sde::SdeCuTaskOp task) { tasks.push_back(task); });
    for (sde::SdeCuTaskOp task : tasks) {
      if (failed(convertCuTaskToCodir(task))) {
        signalPassFailure();
        return;
      }
    }

    SuDepArrayIdPlan depArrayIdPlan;
    if (failed(buildSuDepArrayIdPlan(getOperation(), depArrayIdPlan))) {
      signalPassFailure();
      return;
    }

    SmallVector<sde::SdeSuIterateOp> iterates;
    getOperation().walk(
        [&](sde::SdeSuIterateOp iterate) { iterates.push_back(iterate); });
    for (sde::SdeSuIterateOp iterate : iterates) {
      if (failed(convertSuIterateToCodir(iterate, &depArrayIdPlan))) {
        signalPassFailure();
        return;
      }
    }

    SmallVector<sde::SdeSuDistributeOp> distributes;
    getOperation().walk([&](sde::SdeSuDistributeOp distribute) {
      distributes.push_back(distribute);
    });
    for (sde::SdeSuDistributeOp distribute : distributes) {
      if (failed(inlineSdeSuDistribute(distribute))) {
        signalPassFailure();
        return;
      }
    }

    // Consume committed SDE access-window / redistribution structure onto the
    // codelet graph before the cu_region wrappers (where the carriers live) are
    // inlined away.
    if (failed(consumeCommittedSdeStructure(getOperation()))) {
      signalPassFailure();
      return;
    }

    if (failed(materializeSdeAtomicsToCodir(getOperation()))) {
      signalPassFailure();
      return;
    }

    SmallVector<sde::SdeCuRegionOp> regions;
    getOperation().walk(
        [&](sde::SdeCuRegionOp region) { regions.push_back(region); });
    for (sde::SdeCuRegionOp region : regions) {
      if (failed(inlineSdeCuRegion(region))) {
        signalPassFailure();
        return;
      }
    }

    bool hasUnresolvedMuDep = false;
    getOperation().walk(
        [&](sde::SdeMuDepOp muDep) {
          muDep.emitOpError()
              << "must be consumed by convert-sde-to-codir before the CODIR "
                 "boundary; materialize it as an sde.mu_token/codelet dep or "
                 "remove the stale dependency declaration";
          hasUnresolvedMuDep = true;
        });
    if (hasUnresolvedMuDep)
      signalPassFailure();
  }
};

} // namespace
std::unique_ptr<Pass> mlir::carts::codir::createConvertSdeToCodirPass() {
  return std::make_unique<ConvertSdeToCodirPass>();
}
