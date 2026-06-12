///==========================================================================///
/// File: VerifyArtsObjectsOnly.cpp
///
/// Verification pass for the target SDE-to-ARTS boundary facts.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#define GEN_PASS_DEF_VERIFYARTSOBJECTSONLY
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/StringRef.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

static bool isInsideHostOpenMPIsland(Operation *op) {
  for (Operation *cur = op; cur; cur = cur->getParentOp())
    if (cur->hasAttr(sde::AttrNames::KeepHostOpenMP))
      return true;
  return false;
}

// The "no internode task depends on a coarse aggregate DB" invariant is
// verified in VerifyArtsCdag: it must run AFTER
// Distributed ownership realization marks distributed DB homes and
// DistributedLaunchConsistency localizes host-bridge EDTs to intranode, which
// only happen in post-db-refinement. Checking it here (end of sde-to-arts)
// would reject bridges that are legitimately localized downstream.

static LogicalResult verifyArtsObjectsOnly(ModuleOp module) {
  auto *sdeDialect =
      module->getContext()->getLoadedDialect<sde::CartsSdeDialect>();
  bool found = false;

  module.walk([&](Operation *op) {
    if (sdeDialect && op->getDialect() == sdeDialect) {
      op->emitError() << "SDE operation '" << op->getName()
                      << "' remains after the SDE-to-ARTS boundary";
      found = true;
    }

    if (op->getDialect() && op->getDialect()->getNamespace() == "omp") {
      if (isInsideHostOpenMPIsland(op))
        return;
      op->emitError() << "OpenMP operation '" << op->getName()
                      << "' remains after the SDE-to-ARTS boundary";
      found = true;
    }

    if (isa<scf::ParallelOp>(op)) {
      if (isInsideHostOpenMPIsland(op))
        return;
      op->emitError()
          << "scf.parallel remains after the SDE-to-ARTS boundary; "
             "parallel work must be converted to ARTS objects or marked as "
             "an explicit host OpenMP island";
      found = true;
    }
  });

  return failure(found);
}

struct VerifyArtsObjectsOnlyPass
    : public impl::VerifyArtsObjectsOnlyBase<VerifyArtsObjectsOnlyPass> {
  VerifyArtsObjectsOnlyPass() = default;

  void runOnOperation() override {
    if (failed(verifyArtsObjectsOnly(getOperation())))
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createVerifyArtsObjectsOnlyPass() {
  return std::make_unique<VerifyArtsObjectsOnlyPass>();
}
