///==========================================================================///
/// File: EdtInlineNoDepTasks.cpp
///
/// Inline top-level dep-free EDT bodies into the parent block.
///==========================================================================///

#include "carts/dialect/arts/IR/ArtsDialect.h"
#include "carts/dialect/arts/Utils/EdtUtils.h"
#include "carts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/Statistic.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::arts;

#define GEN_PASS_DEF_EDTINLINENODEPTASKS
#include "carts/passes/Passes.h.inc"

static llvm::Statistic numNoDepEdtsInlinedStat{
    "edt_inline_no_dep_tasks", "NumNoDepEdtsInlined",
    "Number of dep-free EDTs inlined"};

namespace {

struct EdtInlineNoDepTasksPass
    : public ::impl::EdtInlineNoDepTasksBase<EdtInlineNoDepTasksPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<EdtOp, 8> candidates;

    module.walk([&](EdtOp edt) {
      if (edt.getType() != EdtType::task && edt.getType() != EdtType::sync)
        return;
      if (EdtUtils::isInsideEpoch(edt))
        return;
      if (!edt.getDependencies().empty())
        return;
      candidates.push_back(edt);
    });

    for (EdtOp edt : candidates) {
      Block &body = edt.getRegion().front();
      if (body.getNumArguments() != 0)
        continue;

      Operation *insertBefore = edt.getOperation();
      SmallVector<Operation *, 8> opsToMove;
      for (Operation &childOp : body.without_terminator())
        opsToMove.push_back(&childOp);

      for (Operation *childOp : opsToMove)
        childOp->moveBefore(insertBefore);

      edt.erase();
      ++numNoDepEdtsInlinedStat;
    }
  }
};

} // namespace

namespace mlir {
namespace carts::arts {
std::unique_ptr<Pass> createEdtInlineNoDepTasksPass() {
  return std::make_unique<EdtInlineNoDepTasksPass>();
}
} // namespace carts::arts
} // namespace mlir
