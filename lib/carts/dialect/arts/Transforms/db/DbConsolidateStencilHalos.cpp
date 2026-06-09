///==========================================================================///
/// File: DbConsolidateStencilHalos.cpp
///
/// Consolidate DB stencil halo windows.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/LoweringFactUtils.h"
#include "carts/dialect/arts/Utils/StencilAttributes.h"
#include "carts/utils/Utils.h"
#define GEN_PASS_DEF_DBCONSOLIDATESTENCILHALOS
#include "carts/passes/Passes.h"
#include "carts/passes/Passes.h.inc"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/Statistic.h"

using namespace mlir;
using namespace mlir::func;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#include "carts/utils/Debug.h"
ARTS_DEBUG_SETUP(db_consolidate_stencil_halos);

static llvm::Statistic numStencilHalosConsolidated{
    "db_consolidate_stencil_halos", "NumStencilHalosConsolidated",
    "Number of stencil acquires whose halo bounds were consolidated"};

namespace {
static unsigned consolidateStencilHalos(ModuleOp module) {
  unsigned count = 0;

  module.walk([&](func::FuncOp func) {
    SmallVector<DbAcquireOp, 16> acquires;
    func.walk([&](DbAcquireOp acquire) { acquires.push_back(acquire); });

    for (DbAcquireOp acquire : acquires) {
      unsigned rank = 0;
      auto rawMinOffsets = getStencilMinOffsets(acquire.getOperation());
      auto rawMaxOffsets = getStencilMaxOffsets(acquire.getOperation());
      if (rawMinOffsets)
        rank = std::max(rank, static_cast<unsigned>(rawMinOffsets->size()));
      if (rawMaxOffsets)
        rank = std::max(rank, static_cast<unsigned>(rawMaxOffsets->size()));

      auto facts = resolveAcquireFacts(acquire);
      bool usesStencilSemantics =
          (facts && facts->isStencilFamily()) ||
          static_cast<bool>(rawMinOffsets) || static_cast<bool>(rawMaxOffsets);
      if (!usesStencilSemantics)
        continue;

      std::optional<
          std::pair<SmallVector<int64_t, 4>, SmallVector<int64_t, 4>>>
          factsHalo;
      if (facts)
        factsHalo = projectHaloWindow(*facts);
      if (factsHalo) {
        rank =
            std::max(rank, static_cast<unsigned>(factsHalo->first.size()));
        rank =
            std::max(rank, static_cast<unsigned>(factsHalo->second.size()));
      }

      if (rank == 0)
        continue;

      SmallVector<int64_t, 4> unifiedMin(rank, 0);
      SmallVector<int64_t, 4> unifiedMax(rank, 0);

      if (rawMinOffsets)
        for (unsigned d = 0; d < rawMinOffsets->size() && d < rank; ++d)
          unifiedMin[d] = std::min(unifiedMin[d], (*rawMinOffsets)[d]);
      if (rawMaxOffsets)
        for (unsigned d = 0; d < rawMaxOffsets->size() && d < rank; ++d)
          unifiedMax[d] = std::max(unifiedMax[d], (*rawMaxOffsets)[d]);

      if (factsHalo) {
        auto &[factsMins, factsMaxs] = *factsHalo;
        for (unsigned d = 0; d < factsMins.size() && d < rank; ++d)
          unifiedMin[d] = std::min(unifiedMin[d], factsMins[d]);
        for (unsigned d = 0; d < factsMaxs.size() && d < rank; ++d)
          unifiedMax[d] = std::max(unifiedMax[d], factsMaxs[d]);
      }

      bool allZero = true;
      for (unsigned d = 0; d < rank; ++d) {
        if (unifiedMin[d] != 0 || unifiedMax[d] != 0) {
          allZero = false;
          break;
        }
      }
      if (allZero)
        continue;

      OpBuilder builder(acquire.getContext());
      builder.setInsertionPointAfter(acquire.getOperation());
      Operation *acquireOp = acquire.getOperation();
      acquire.setDepPatternAttr(
          ArtsDepPatternAttr::get(acquire.getContext(), ArtsDepPattern::stencil));
      acquire.setDistributionPatternAttr(EdtDistributionPatternAttr::get(
          acquire.getContext(), EdtDistributionPattern::stencil));
      acquireOp->setAttr(acquire.getStencilMinOffsetsAttrName(),
                         builder.getI64ArrayAttr(unifiedMin));
      acquireOp->setAttr(acquire.getStencilMaxOffsetsAttrName(),
                         builder.getI64ArrayAttr(unifiedMax));

      ++count;
      ARTS_DEBUG("consolidated halo for acquire " << acquire);
    }
  });

  return count;
}

struct DbConsolidateStencilHalosPass
    : public impl::DbConsolidateStencilHalosBase<
          DbConsolidateStencilHalosPass> {
  DbConsolidateStencilHalosPass() = default;

  void runOnOperation() override {
    ARTS_INFO_HEADER(DbConsolidateStencilHalosPass);
    unsigned count = consolidateStencilHalos(getOperation());
    numStencilHalosConsolidated += count;
    if (count > 0)
      ARTS_INFO("consolidated stencil halos on " << count << " acquires");
    ARTS_INFO_FOOTER(DbConsolidateStencilHalosPass);
  }
};
} // namespace

std::unique_ptr<Pass>
mlir::carts::arts::createDbConsolidateStencilHalosPass() {
  return std::make_unique<DbConsolidateStencilHalosPass>();
}
