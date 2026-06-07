///==========================================================================///
/// File: VerifySdeCoarseAvoidance.cpp
///
/// Coarse MU shape is a diagnosed last resort, never silent.
///
/// For every `sde.mu_alloc` that is NOT block-partitioned (no block grid in its
/// type), this verifier fails closed with a reason:
///
///   * in the single-owner block-grid realize scope (a committed plan proving a
///     real grid) but left flat — an AVOIDABLE coarse MU `sde-coarse-avoidance`
///     should have rank-expanded;
///   * otherwise an unsupported coarse case (dynamic / in-place / reduction /
///     matmul / multi-owner / aliasing) — a last resort that cannot be
///     block-partitioned in the current IR, surfaced with evidence instead of
///     silently.
///
/// An MU with no committed plan and no coarse-relevant structure (e.g. private
/// scratch) is legitimately non-distributed and accepted. The verifier reads
/// the committed plan verbatim and shares the realize gate
/// (`isSingleOwnerBlockGridRealizable`) with rank expansion and the pass; it
/// never recomputes owner dims or block shape.
///==========================================================================///

#include "carts/dialect/sde/IR/SdeDialect.h"
#include "carts/dialect/sde/Transforms/Passes.h"
#include "carts/dialect/sde/Utils/MuLayout.h"
#include "carts/dialect/sde/Utils/MuLayoutRewriter.h"

namespace mlir::carts::sde {
#define GEN_PASS_DEF_VERIFYSDECOARSEAVOIDANCE
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

using namespace mlir;
using namespace mlir::carts;

namespace {

/// Why a flat MU cannot be block-partitioned. Each non-None reason is a
/// diagnosed last resort the current IR cannot avoid.
enum class CoarseReason {
  None,       ///< no committed plan and no coarse-relevant structure -> accept
  Dynamic,    ///< dynamic shape: no static block grid
  InPlace,    ///< in-place read+write: no independent single-writer blocks
  Reduction,  ///< reduction carrier: no single-writer block grain
  Matmul,     ///< matmul contraction needs cross-tile redistribution
  MultiOwner, ///< multi-owner block plan: realization not supported here
  AliasingUse ///< unsupported/aliasing use of the MU root blocks realization
};

/// Nearest `sde.su_iterate` enclosing any load/store of `mu`, regardless of a
/// committed block plan — used only to LABEL the coarse reason.
static carts::sde::SdeSuIterateOp
findAccessSuIterate(carts::sde::SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers()) {
    if (isa<memref::LoadOp, memref::StoreOp>(user))
      if (auto si = user->getParentOfType<carts::sde::SdeSuIterateOp>())
        return si;
  }
  return nullptr;
}

/// True if the MU's redistribution is explicitly represented by an sde.redist
/// fact — its movement is owned by sde-redistribute, not a coarse last resort.
static bool muHasRedist(carts::sde::SdeMuAllocOp mu) {
  for (Operation *user : mu.getMemref().getUsers())
    if (isa<carts::sde::SdeRedistOp>(user))
      return true;
  return false;
}

static CoarseReason classifyCoarseReason(carts::sde::SdeMuAllocOp mu,
                                         MemRefType logicalType) {
  if (!logicalType.hasStaticShape())
    return CoarseReason::Dynamic;
  if (carts::sde::SdeSuIterateOp si = findAccessSuIterate(mu)) {
    if (si.getInPlaceSafe())
      return CoarseReason::InPlace;
    if (std::optional<carts::sde::SdeStructuredClassification> cls =
            si.getStructuredClassification()) {
      if (*cls == carts::sde::SdeStructuredClassification::reduction)
        return CoarseReason::Reduction;
      if (*cls == carts::sde::SdeStructuredClassification::matmul)
        return CoarseReason::Matmul;
    }
    if (ArrayAttr owner = si.getPhysicalOwnerDimsAttr())
      if (owner.size() > 1)
        return CoarseReason::MultiOwner;
  }
  if (carts::sde::muRootHasUnsupportedUse(mu.getMemref()))
    return CoarseReason::AliasingUse;
  return CoarseReason::None;
}

static StringRef coarseReasonText(CoarseReason r) {
  switch (r) {
  case CoarseReason::Dynamic:
    return "dynamic MU shape has no static block grid; conservative coarse "
           "allocation is the last resort";
  case CoarseReason::InPlace:
    return "in-place read+write of one MU exposes no independent single-writer "
           "blocks; conservative coarse allocation is the last resort";
  case CoarseReason::Reduction:
    return "reduction carrier has no single-writer block grain; conservative "
           "coarse allocation is the last resort";
  case CoarseReason::Matmul:
    return "matmul contraction needs cross-tile redistribution before "
           "single-writer blocks exist; conservative coarse allocation is the "
           "last resort, no redistribution invented";
  case CoarseReason::MultiOwner:
    return "multi-owner block plan realization is not supported here; "
           "conservative coarse allocation is the last resort";
  case CoarseReason::AliasingUse:
    return "unsupported/aliasing use of the MU root blocks block-grid "
           "realization; conservative coarse allocation is the last resort";
  case CoarseReason::None:
    break;
  }
  return "";
}

struct VerifySdeCoarseAvoidancePass
    : public sde::impl::VerifySdeCoarseAvoidanceBase<
          VerifySdeCoarseAvoidancePass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    bool failed = false;

    module.walk(
        [&](sde::SdeMuAllocOp mu) {
          auto muType = dyn_cast<MemRefType>(mu.getMemref().getType());
          if (!muType)
            return;
          // Redistribution explicitly represented: sde-redistribute owns this
          // MU's movement, so it is not a coarse last resort.
          // verify-sde-redistribute gates the redistribution structure itself.
          if (muHasRedist(mu))
            return;
          sde::SdeSuIterateOp si = sde::findCommittedBlockPlanWriter(mu);

          // Block-partitioned: the grid is in the type. OK.
          if (si && sde::recognizeExpandedBlockGridMu(si, muType))
            return;

          // Avoidable coarse: in the realize scope but left flat.
          sde::MuPhysicalLayout plan;
          if (sde::isSingleOwnerBlockGridRealizable(si, muType, plan)) {
            mu.emitOpError()
                << "MU is in the block-grid realize scope but left coarse; "
                   "sde-coarse-avoidance must rank-expand the committed finest "
                   "grain";
            failed = true;
            return;
          }

          // Unsupported coarse: diagnose the last-resort reason, never silent.
          CoarseReason reason = classifyCoarseReason(mu, muType);
          if (reason != CoarseReason::None) {
            mu.emitOpError() << coarseReasonText(reason);
            failed = true;
          }
          // None -> legitimately non-distributed -> accept.
        });

    if (failed)
      signalPassFailure();
  }
};

} // namespace

namespace mlir::carts::sde {
std::unique_ptr<Pass> createVerifySdeCoarseAvoidancePass() {
  return std::make_unique<VerifySdeCoarseAvoidancePass>();
}
} // namespace mlir::carts::sde
