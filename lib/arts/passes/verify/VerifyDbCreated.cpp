///==========================================================================///
/// File: VerifyDbCreated.cpp
///
/// Informational verification pass that checks whether DB allocations exist
/// after the create-dbs stage. Warns if EDTs are present but no DBs were
/// created, which may indicate a pattern discovery or CreateDbs issue.
///==========================================================================///

#include "arts/Dialect.h"
#define GEN_PASS_DEF_VERIFYDBCREATED
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/Debug.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using mlir::arts::debugStream;

ARTS_DEBUG_SETUP(verify_db_created);

namespace {
struct VerifyDbCreatedPass
    : public impl::VerifyDbCreatedBase<VerifyDbCreatedPass> {
  void runOnOperation() override {
    auto module = getOperation();
    bool hasEdts = false;
    bool hasDbs = false;
    module.walk([&](arts::EdtOp) { hasEdts = true; });
    module.walk([&](arts::DbAllocOp) { hasDbs = true; });
    if (hasEdts && !hasDbs)
      ARTS_WARN("EDTs found but no DB allocations after create-dbs stage");
  }
};
} // namespace

std::unique_ptr<Pass> mlir::arts::createVerifyDbCreatedPass() {
  return std::make_unique<VerifyDbCreatedPass>();
}
