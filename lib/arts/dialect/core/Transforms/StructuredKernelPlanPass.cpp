///==========================================================================///
/// File: StructuredKernelPlanPass.cpp
///
/// Module pass that runs StructuredKernelPlanAnalysis and stamps plan
/// attributes on ForOp/EdtOp. Downstream passes read these attributes;
/// if absent, they fall back to existing generic logic.
///==========================================================================///

#define GEN_PASS_DEF_STRUCTUREDKERNELPLAN
#include "arts/Dialect.h"
#include "arts/dialect/core/Analysis/AnalysisDependencies.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Analysis/heuristics/StructuredKernelPlanAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(structured_kernel_plan);

#include "llvm/ADT/Statistic.h"

static llvm::Statistic numPlansStamped{
    "structured_kernel_plan", "NumPlansStamped",
    "Number of ForOps with structured kernel plan stamped"};
static llvm::Statistic numPlansRejected{
    "structured_kernel_plan", "NumPlansRejected",
    "Number of ForOps where plan synthesis was skipped"};

using namespace mlir::arts;

static const AnalysisKind kStructuredKernelPlan_reads[] = {
    AnalysisKind::DbAnalysis, AnalysisKind::LoopAnalysis,
    AnalysisKind::AbstractMachine};
[[maybe_unused]] static const AnalysisDependencyInfo
    kStructuredKernelPlan_deps = {kStructuredKernelPlan_reads, {}};

using namespace mlir;

namespace {

struct StructuredKernelPlanPass
    : public impl::StructuredKernelPlanBase<StructuredKernelPlanPass> {
  explicit StructuredKernelPlanPass(mlir::arts::AnalysisManager *AM) : AM(AM) {
    assert(AM && "AnalysisManager must be provided externally");
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    auto &planAnalysis = AM->getStructuredKernelPlanAnalysis();
    planAnalysis.run(module);

    /// Stamp plan attributes on ForOps and EdtOps.
    module.walk([&](ForOp forOp) {
      const auto *plan = planAnalysis.getPlan(forOp);
      if (!plan || !plan->isValid()) {
        ++numPlansRejected;
        return;
      }
      stampPlanAttrs(forOp.getOperation(), *plan);
      ++numPlansStamped;
    });

    module.walk([&](EdtOp edtOp) {
      const auto *plan = planAnalysis.getPlan(edtOp);
      if (!plan || !plan->isValid())
        return;
      stampPlanAttrs(edtOp.getOperation(), *plan);
    });
  }

private:
  void stampPlanAttrs(Operation *op, const StructuredKernelPlan &plan) {
    auto *ctx = op->getContext();

    op->setAttr(AttrNames::Operation::Plan::KernelFamily,
                StringAttr::get(ctx, kernelFamilyToString(plan.family)));
    op->setAttr(AttrNames::Operation::Plan::IterationTopology,
                StringAttr::get(ctx, iterationTopologyToString(plan.topology)));
    op->setAttr(
        AttrNames::Operation::Plan::AsyncStrategy,
        StringAttr::get(ctx, asyncStrategyToString(plan.asyncStrategy)));
    op->setAttr(
        AttrNames::Operation::Plan::RepetitionStructure,
        StringAttr::get(ctx, repetitionStructureToString(plan.repetition)));

    if (!plan.ownerDims.empty())
      op->setAttr(AttrNames::Operation::Plan::OwnerDims,
                  buildI64ArrayAttr(op, plan.ownerDims));
    if (!plan.physicalBlockShape.empty())
      op->setAttr(AttrNames::Operation::Plan::PhysicalBlockShape,
                  buildI64ArrayAttr(op, plan.physicalBlockShape));
    if (!plan.logicalWorkerSlice.empty())
      op->setAttr(AttrNames::Operation::Plan::LogicalWorkerSlice,
                  buildI64ArrayAttr(op, plan.logicalWorkerSlice));
    if (!plan.haloShape.empty())
      op->setAttr(AttrNames::Operation::Plan::HaloShape,
                  buildI64ArrayAttr(op, plan.haloShape));

    /// Cost signals.
    auto i64Type = IntegerType::get(ctx, 64);
    auto costToAttr = [&](double val) -> IntegerAttr {
      return IntegerAttr::get(i64Type, static_cast<int64_t>(val * 1000.0));
    };
    op->setAttr(AttrNames::Operation::Plan::CostSchedulerOverhead,
                costToAttr(plan.costSignals.schedulerOverhead));
    op->setAttr(AttrNames::Operation::Plan::CostSliceWideningPressure,
                costToAttr(plan.costSignals.sliceWideningPressure));
    op->setAttr(AttrNames::Operation::Plan::CostExpectedLocalWork,
                costToAttr(plan.costSignals.expectedLocalWork));
    op->setAttr(AttrNames::Operation::Plan::CostRelaunchAmortization,
                costToAttr(plan.costSignals.relaunchAmortization));

    ARTS_DEBUG(
        "Stamped plan on op: family=" << kernelFamilyToString(plan.family));
  }

  mlir::arts::AnalysisManager *AM = nullptr;
};

} // namespace

namespace mlir {
namespace arts {
std::unique_ptr<Pass>
createStructuredKernelPlanPass(mlir::arts::AnalysisManager *AM) {
  return std::make_unique<StructuredKernelPlanPass>(AM);
}
} // namespace arts
} // namespace mlir
