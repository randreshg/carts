///==========================================================================///
/// File: DbCommitDistributedDeps.cpp
///
/// Commits the runtime DB mode and per-slot halo window on distributed DB
/// acquires so ARTS-RT lowering reads committed facts with no fallback
/// inference. Mechanical: it only fills missing committed facts and never
/// recomputes an existing mode verdict.
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

/// Project the committed stencil access window into a per-slot halo_slice so
/// downstream consumers read a committed halo fact instead of re-deriving it.
/// Covers every distributed partial halo acquire ARTS-RT would otherwise
/// reconstruct (stencil partition mode or block-halo signal), not just acquires
/// whose partition mode is literally `stencil`.
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
