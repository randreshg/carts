///==========================================================================///
/// File: RealizeEdtDistribution.cpp
///
/// Authors ARTS EDT distribution facts from committed graph structure.
/// The ARTS boundary emits explicit deps and access windows; this pass records
/// the distribution family, version, and block-halo capability marker.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/DistributedDbPlacementUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#define GEN_PASS_DEF_REALIZEEDTDISTRIBUTION
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

static LogicalResult realizePerBlockHaloDependencies(arts::EdtOp edt) {
  bool canRealizeHaloDeps =
      edt->hasAttr(edt.getStencilSupportedBlockHaloAttrName());

  bool hasCommittedHaloRead = false;
  for (Value dep : edt.getDependencies()) {
    auto acquire = dyn_cast_or_null<arts::DbAcquireOp>(
        arts::DbUtils::getUnderlyingDb(dep));
    if (acquire)
      acquire.removeHaloViewDependencyAttr();
    if (!canRealizeHaloDeps)
      continue;
    if (!acquire || acquire.getMode() != arts::ArtsMode::in ||
        !arts::DbUtils::acquiresPartialHaloWindow(acquire))
      continue;

    if (!arts::DbUtils::hasCommittedDbSpaceWindow(acquire))
      return acquire.emitOpError()
             << "realizes a per-block halo dependency without explicit "
                "element_offsets/element_sizes; ARTS-RT must not infer halo "
                "byte windows";
    acquire.setHaloViewDependencyAttr(UnitAttr::get(acquire.getContext()));
    auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
        arts::DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
    if (alloc && arts::hasArtsDbPhysicalLayout(alloc.getOperation()))
      alloc.setPerBlockSingleWriterStencilAttr(
          UnitAttr::get(alloc.getContext()));
    hasCommittedHaloRead = true;
  }

  if (hasCommittedHaloRead)
    edt.setPerBlockHaloExchangeAttr(UnitAttr::get(edt.getContext()));
  else
    edt.removePerBlockHaloExchangeAttr();

  return success();
}

struct RealizeEdtDistributionPass
    : public impl::RealizeEdtDistributionBase<RealizeEdtDistributionPass> {
  void runOnOperation() override {
    WalkResult result = getOperation().walk([](arts::EdtOp edt) {
      std::optional<arts::ArtsDepPattern> pattern =
          arts::getDepPattern(edt.getOperation());
      if (!pattern || *pattern == arts::ArtsDepPattern::unknown)
        return WalkResult::advance();

      Operation *op = edt.getOperation();
      if (std::optional<arts::EdtDistributionPattern> family =
              arts::getDistributionPatternForDepPattern(*pattern)) {
        arts::setEdtDistributionPattern(op, *family);
        arts::setDistributionVersion(op, 1);
      }

      // A stencil-family dep with a committed access window reads neighbor
      // tiles out of a per-block DB, so it advertises block-halo capability.
      if (arts::isStencilFamilyDepPattern(*pattern) &&
          edt->hasAttr(edt.getStencilMinOffsetsAttrName()) &&
          edt->hasAttr(edt.getStencilMaxOffsetsAttrName()))
        edt->setAttr(edt.getStencilSupportedBlockHaloAttrName(),
                     UnitAttr::get(edt.getContext()));

      if (failed(realizePerBlockHaloDependencies(edt)))
        return WalkResult::interrupt();
      return WalkResult::advance();
    });
    if (result.wasInterrupted())
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createRealizeEdtDistributionPass() {
  return std::make_unique<RealizeEdtDistributionPass>();
}
