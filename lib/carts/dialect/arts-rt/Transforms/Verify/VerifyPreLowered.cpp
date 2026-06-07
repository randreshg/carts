///==========================================================================///
/// File: VerifyPreLowered.cpp
///
/// Fail-closed gate run at the end of pre-lowering, immediately before the
/// irreversible ARTS-RT-to-LLVM conversion.
///
/// ARTS-RT lowers committed ARTS facts mechanically; it never infers placement,
/// ownership, partition, or runtime-mode policy. This pass rejects residual
/// high-level scheduler ops and surviving DB ops that reach ABI lowering
/// without the committed facts ARTS-RT consumes. It checks facts the op
/// verifiers do not already enforce: partition_mode, and the runtime DB mode on
/// distributed acquires. A dropped fact surfaces here as a diagnostic instead
/// of being reinvented during LLVM lowering.
///==========================================================================///

#include "carts/dialect/arts-rt/Transforms/Passes.h"
#include "carts/dialect/arts-rt/Utils/RtDbUtils.h"
#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/OperationAttributes.h"
namespace mlir::carts::arts_rt {
#define GEN_PASS_DEF_VERIFYPRELOWERED
#include "carts/dialect/arts-rt/Transforms/Passes.h.inc"
} // namespace mlir::carts::arts_rt
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

namespace {
struct VerifyPreLoweredPass
    : public arts_rt::impl::VerifyPreLoweredBase<VerifyPreLoweredPass> {
  void runOnOperation() override {
    bool found = false;
    getOperation().walk([&](Operation *op) {
      if (isa<arts::EdtOp, arts::EpochOp>(op)) {
        op->emitError(
            "high-level scheduler op survived past pre-lowering step");
        found = true;
        return;
      }
      if (auto alloc = dyn_cast<arts::DbAllocOp>(op)) {
        if (!alloc.getPartitionMode()) {
          alloc.emitOpError()
              << "missing required partition_mode before ABI lowering";
          found = true;
        }
        return;
      }
      if (auto acquire = dyn_cast<arts::DbAcquireOp>(op)) {
        if (!acquire.getPartitionMode()) {
          acquire.emitOpError()
              << "missing required partition_mode before ABI lowering";
          found = true;
          return;
        }
        // Distributed acquires must carry ARTS' committed RO/EW/RW verdict.
        auto alloc = dyn_cast_or_null<arts::DbAllocOp>(
            arts_rt::RtDbUtils::getUnderlyingDbAlloc(acquire.getSourcePtr()));
        if (alloc && hasDistributedDbAllocation(alloc.getOperation()) &&
            !acquire.getRuntimeDbMode()) {
          acquire.emitOpError()
              << "distributed DB acquire reached ABI lowering without a "
                 "committed runtime DB mode; ARTS-RT must not infer it";
          found = true;
        }
        return;
      }
    });
    if (found)
      signalPassFailure();
  }
};
} // namespace

std::unique_ptr<Pass> mlir::carts::arts_rt::createVerifyPreLoweredPass() {
  return std::make_unique<VerifyPreLoweredPass>();
}
