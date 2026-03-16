///==========================================================================///
/// File: VerifyDbCreated.cpp
///
/// Informational verification pass that checks whether DB allocations exist
/// after the create-dbs stage. Warns if EDTs are present but no DBs were
/// created, which may indicate a pattern discovery or CreateDbs issue.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;

namespace {
struct VerifyDbCreatedPass
    : public arts::VerifyDbCreatedBase<VerifyDbCreatedPass> {
  void runOnOperation() override {
    auto module = getOperation();
    bool hasEdts = false;
    bool hasDbs = false;
    module.walk([&](arts::EdtOp) { hasEdts = true; });
    module.walk([&](arts::DbAllocOp) { hasDbs = true; });
    if (hasEdts && !hasDbs)
      llvm::errs() << "[verify-db-created] Warning: EDTs found but no DB "
                      "allocations after create-dbs stage\n";
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifyDbCreatedPass() {
  return std::make_unique<VerifyDbCreatedPass>();
}
