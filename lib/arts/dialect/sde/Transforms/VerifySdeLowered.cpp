///==========================================================================///
/// File: VerifySdeLowered.cpp
///
/// Verification pass that ensures no sde.* operations and no transient
/// linalg/tensor carriers survive after SDE-to-ARTS conversion. Any
/// remaining SDE ops or carriers indicate a conversion failure.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/dialect/sde/IR/SdeDialect.h"
#include "arts/dialect/sde/Transforms/Passes.h"
namespace mlir::arts {
#define GEN_PASS_DEF_VERIFYSDELOWERED
#include "arts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::arts
#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/Tensor/IR/Tensor.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {

/// Returns true if the op is a transient carrier that should have been erased
/// by ConvertSdeToArts.
static bool isOrphanedCarrierOp(Operation *op) {
  // Any linalg op nested under an arts.for is a transient carrier that should
  // have been erased by SDE->ARTS lowering, regardless of whether it is
  // still memref-backed or already tensor-backed.
  if (isa<linalg::LinalgOp>(op))
    return op->getParentOfType<arts::ForOp>() != nullptr;

  // bufferization.to_tensor inside arts.for is a carrier chain remnant.
  if (isa<bufferization::ToTensorOp>(op))
    return static_cast<bool>(op->getParentOfType<arts::ForOp>());

  // tensor.empty inside arts.for is a carrier chain remnant.
  if (isa<tensor::EmptyOp>(op))
    return static_cast<bool>(op->getParentOfType<arts::ForOp>());

  return false;
}

struct VerifySdeLoweredPass
    : public arts::impl::VerifySdeLoweredBase<VerifySdeLoweredPass> {
  void runOnOperation() override {
    auto *sdeDialect = getOperation()
                           ->getContext()
                           ->getLoadedDialect<arts::sde::ArtsSdeDialect>();
    bool failed = false;
    getOperation().walk([&](Operation *op) {
      if (sdeDialect && op->getDialect() == sdeDialect) {
        op->emitError() << "SDE operation '" << op->getName()
                        << "' survived past SDE-to-ARTS conversion";
        failed = true;
      }
      if (isOrphanedCarrierOp(op)) {
        op->emitError() << "transient carrier '" << op->getName()
                        << "' survived past SDE-to-ARTS conversion";
        failed = true;
      }
    });
    if (failed)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::sde::createVerifySdeLoweredPass() {
  return std::make_unique<VerifySdeLoweredPass>();
}
