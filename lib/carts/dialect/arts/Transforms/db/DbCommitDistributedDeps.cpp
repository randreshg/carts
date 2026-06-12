///==========================================================================///
/// File: DbCommitDistributedDeps.cpp
///
/// Commits runtime DB mode and stencil halo diagnostics on distributed DB
/// acquires. ARTS-RT still requires explicit element/byte windows for halo
/// transport; halo_slice alone is not lowering authority.
///==========================================================================///

#define GEN_PASS_DEF_DBCOMMITDISTRIBUTEDDEPS
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/DbUtils.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "carts/utils/ArrayAttrUtils.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {

/// Project the committed stencil access window into a per-slot halo_slice for
/// verification and diagnostics. It must not be used by ARTS-RT to synthesize
/// byte windows.
static void commitHaloSlice(DbAcquireOp acquire) {
  if (acquire.getHaloSliceAttr() ||
      !DbUtils::acquiresPartialHaloWindow(acquire))
    return;
  auto lower = readI64ArrayAttr(acquire.getStencilMinOffsetsAttr());
  auto upper = readI64ArrayAttr(acquire.getStencilMaxOffsetsAttr());
  if (!lower || lower->empty() || !upper || upper->empty() ||
      lower->size() != upper->size())
    return;
  MLIRContext *ctx = acquire.getContext();
  acquire.setHaloSliceAttr(
      HaloSliceAttr::get(ctx, DenseI64ArrayAttr::get(ctx, *lower),
                         DenseI64ArrayAttr::get(ctx, *upper)));
}

struct DbCommitDistributedDepsPass
    : public impl::DbCommitDistributedDepsBase<DbCommitDistributedDepsPass> {
  void runOnOperation() override {
    getOperation().walk([&](DbAcquireOp acquire) {
      auto alloc = dyn_cast_or_null<DbAllocOp>(
          DbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
      if (!alloc || !hasDistributedDbAllocation(alloc.getOperation()))
        return;
      if (!acquire.getRuntimeDbMode())
        acquire.setRuntimeDbModeAttr(RuntimeDbModeAttr::get(
            acquire.getContext(),
            DbUtils::orderedRuntimeDbMode(acquire.getMode())));
      commitHaloSlice(acquire);
    });
  }
};

} // namespace

std::unique_ptr<Pass> mlir::carts::arts::createDbCommitDistributedDepsPass() {
  return std::make_unique<DbCommitDistributedDepsPass>();
}
