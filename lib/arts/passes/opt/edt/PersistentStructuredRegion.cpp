///==========================================================================///
/// File: PersistentStructuredRegion.cpp
///
/// Pass that identifies proven regular kernels eligible for persistent
/// structured region lowering and stamps the necessary attributes.
///
/// For kernels that pass both the ownership proof gate and the cost model
/// gate, this pass stamps `arts.persistent_region = true` on the enclosing
/// epoch. Downstream lowering (EpochLowering) consults this attribute to
/// select the persistent region execution path.
///
/// Gate requirements:
///   1. Fully proven ownership proof on all LoweringContractOps within
///      the epoch's worker EDTs
///   2. Structured kernel plan with repetition structure
///   3. Cost model indicates benefit from persistence
///
/// When the gate is not met, the epoch is left unchanged and follows the
/// default per-step lowering path.
///==========================================================================///

#define GEN_PASS_DEF_PERSISTENTSTRUCTUREDREGION
#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/db/OwnershipProof.h"
#include "arts/analysis/heuristics/PersistentRegionCostModel.h"
#include "arts/analysis/heuristics/StructuredKernelPlanAnalysis.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(persistent_structured_region);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct PersistentStructuredRegionPass
    : public impl::PersistentStructuredRegionBase<
          PersistentStructuredRegionPass> {

  PersistentStructuredRegionPass() = default;
  explicit PersistentStructuredRegionPass(arts::AnalysisManager *AM) : AM(AM) {}

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(PersistentStructuredRegionPass);

    unsigned totalEpochs = 0;
    unsigned eligibleEpochs = 0;
    unsigned persistentEpochs = 0;

    unsigned workerCount = 1;
    if (AM) {
      auto &machine = AM->getAbstractMachine();
      workerCount = machine.getRuntimeTotalWorkers();
    }

    module.walk([&](EpochOp epoch) {
      ++totalEpochs;

      /// Collect all worker EDTs within this epoch.
      SmallVector<EdtOp> workerEdts;
      epoch.walk([&](EdtOp edt) { workerEdts.push_back(edt); });
      if (workerEdts.empty())
        return;

      /// Check if any worker EDT has a structured kernel plan.
      const StructuredKernelPlan *plan = nullptr;
      for (EdtOp edt : workerEdts) {
        auto familyAttr = edt->getAttrOfType<StringAttr>(
            AttrNames::Operation::Plan::KernelFamily);
        if (!familyAttr)
          continue;

        /// Reconstruct plan from attrs.
        auto family = parseKernelFamily(familyAttr.getValue());
        if (!family || *family == KernelFamily::Unknown)
          continue;

        /// Build a local plan from the stamped attributes.
        static thread_local StructuredKernelPlan localPlan;
        localPlan = StructuredKernelPlan();
        localPlan.family = *family;

        if (auto topoAttr = edt->getAttrOfType<StringAttr>(
                AttrNames::Operation::Plan::IterationTopology))
          if (auto topo = parseIterationTopology(topoAttr.getValue()))
            localPlan.topology = *topo;

        if (auto stratAttr = edt->getAttrOfType<StringAttr>(
                AttrNames::Operation::Plan::AsyncStrategy))
          if (auto strat = parseAsyncStrategy(stratAttr.getValue()))
            localPlan.asyncStrategy = *strat;

        if (auto repAttr = edt->getAttrOfType<StringAttr>(
                AttrNames::Operation::Plan::RepetitionStructure))
          if (auto rep = parseRepetitionStructure(repAttr.getValue()))
            localPlan.repetition = *rep;

        /// Read cost signals (stored as milliunits).
        auto readCost = [&](StringRef name) -> double {
          if (auto attr = edt->getAttrOfType<IntegerAttr>(name))
            return attr.getInt() / 1000.0;
          return 0.0;
        };
        localPlan.costSignals.schedulerOverhead =
            readCost(AttrNames::Operation::Plan::CostSchedulerOverhead);
        localPlan.costSignals.sliceWideningPressure =
            readCost(AttrNames::Operation::Plan::CostSliceWideningPressure);
        localPlan.costSignals.relaunchAmortization =
            readCost(AttrNames::Operation::Plan::CostRelaunchAmortization);

        plan = &localPlan;
        break;
      }

      if (!plan)
        return;

      /// Check ownership proofs on all LoweringContractOps within the epoch.
      OwnershipProof worstProof;
      worstProof.ownerDimReachability = true;
      worstProof.partitionAccessMapping = true;
      worstProof.haloLegality = true;
      worstProof.depSliceSoundness = true;
      worstProof.relaunchStateSoundness = true;

      epoch.walk([&](LoweringContractOp contract) {
        OwnershipProof proof = readOwnershipProof(contract.getOperation());
        if (!proof.ownerDimReachability)
          worstProof.ownerDimReachability = false;
        if (!proof.partitionAccessMapping)
          worstProof.partitionAccessMapping = false;
        if (!proof.haloLegality)
          worstProof.haloLegality = false;
        if (!proof.depSliceSoundness)
          worstProof.depSliceSoundness = false;
        if (!proof.relaunchStateSoundness)
          worstProof.relaunchStateSoundness = false;
      });

      ++eligibleEpochs;

      /// Evaluate the cost model gate.
      PersistentRegionGate gate =
          evaluatePersistentRegionGate(*plan, worstProof, workerCount);

      ARTS_DEBUG("Epoch " << epoch->getLoc()
                          << ": proof=" << worstProof.provenCount() << "/5"
                          << " gate=" << (gate.shouldPersist ? "PASS" : "FAIL")
                          << " overhead=" << gate.launchOverheadRatio
                          << " repetition=" << gate.repetitionBenefit
                          << " stability=" << gate.sliceStability);

      if (gate.shouldPersist) {
        epoch->setAttr(AttrNames::Operation::PersistentRegion,
                       BoolAttr::get(epoch.getContext(), true));
        /// Propagate plan family to epoch for downstream lowering.
        epoch->setAttr(AttrNames::Operation::Plan::KernelFamily,
                       StringAttr::get(epoch.getContext(),
                                       kernelFamilyToString(plan->family)));
        ++persistentEpochs;
      }
    });

    ARTS_INFO("Persistent region summary: "
              << totalEpochs << " epochs, " << eligibleEpochs << " eligible, "
              << persistentEpochs << " marked persistent");

    ARTS_INFO_FOOTER(PersistentStructuredRegionPass);
  }

  arts::AnalysisManager *AM = nullptr;
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createPersistentStructuredRegionPass(arts::AnalysisManager *AM) {
  return std::make_unique<PersistentStructuredRegionPass>(AM);
}
