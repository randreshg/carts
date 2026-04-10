///==========================================================================///
/// File: StructuredKernelPlanAnalysis.cpp
///
/// Plan synthesis: classify kernel family from depPattern + access patterns,
/// compute ownerDims/blockShape/haloShape, select topology/strategy, compute
/// cost signals. If classification fails, no plan is stored and downstream
/// passes use their existing generic paths.
///==========================================================================///

#include "arts/dialect/core/Analysis/heuristics/StructuredKernelPlanAnalysis.h"
#include "arts/dialect/core/Analysis/AnalysisManager.h"
#include "arts/dialect/core/Analysis/db/DbAnalysis.h"
#include "arts/dialect/core/Analysis/loop/LoopAnalysis.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(structured_kernel_plan);

using namespace mlir;
using namespace mlir::arts;

namespace {

constexpr double kMaxEstimatedWork = 1.0e12;

double saturatingMul(double lhs, double rhs) {
  if (lhs <= 0.0 || rhs <= 0.0)
    return 0.0;
  return std::min(lhs * rhs, kMaxEstimatedWork);
}

double estimateBlockWork(Block &block);

double estimateRegionWork(Region &region) {
  double maxBlockWork = 0.0;
  for (Block &block : region)
    maxBlockWork = std::max(maxBlockWork, estimateBlockWork(block));
  return maxBlockWork;
}

double estimateOpWork(Operation &op) {
  if (op.hasTrait<OpTrait::IsTerminator>())
    return 0.0;

  double nestedRegionWork = 0.0;
  if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
    nestedRegionWork = std::max(estimateRegionWork(ifOp.getThenRegion()),
                                estimateRegionWork(ifOp.getElseRegion()));
  } else {
    for (Region &region : op.getRegions())
      nestedRegionWork += estimateRegionWork(region);
  }

  if (auto tripCount = arts::getStaticTripCount(&op);
      tripCount && *tripCount > 0 && op.getNumRegions() > 0) {
    return saturatingMul(static_cast<double>(*tripCount),
                         std::max(nestedRegionWork, 1.0));
  }

  if (nestedRegionWork > 0.0)
    return std::max(1.0, nestedRegionWork);

  return 1.0;
}

double estimateBlockWork(Block &block) {
  double blockWork = 0.0;
  for (Operation &op : block)
    blockWork = std::min(kMaxEstimatedWork, blockWork + estimateOpWork(op));
  return blockWork;
}

int64_t getStructuredWorkerCount(ForOp forOp) {
  if (auto workers = getWorkers(forOp.getOperation()); workers && *workers > 0)
    return *workers;
  if (auto workersPerNode = getWorkersPerNode(forOp.getOperation());
      workersPerNode && *workersPerNode > 0) {
    return *workersPerNode;
  }

  for (Operation *parent = forOp->getParentOp(); parent;
       parent = parent->getParentOp()) {
    if (auto workers = getWorkers(parent); workers && *workers > 0)
      return *workers;
    if (auto workersPerNode = getWorkersPerNode(parent);
        workersPerNode && *workersPerNode > 0) {
      return *workersPerNode;
    }
  }

  return 1;
}

double estimateExpectedLocalWork(ForOp forOp) {
  auto tripCount = arts::getStaticTripCount(forOp.getOperation());
  if (!tripCount || *tripCount <= 0)
    return 0.0;

  double perIterationWork = estimateBlockWork(*forOp.getBody());
  if (perIterationWork <= 0.0)
    return 0.0;

  double totalLoopWork =
      saturatingMul(static_cast<double>(*tripCount), perIterationWork);
  int64_t workers = std::max<int64_t>(1, getStructuredWorkerCount(forOp));
  return std::max(1.0, totalLoopWork / static_cast<double>(workers));
}

} // namespace

///===----------------------------------------------------------------------===///
/// String conversion helpers
///===----------------------------------------------------------------------===///

StringRef mlir::arts::kernelFamilyToString(KernelFamily family) {
  switch (family) {
  case KernelFamily::Uniform:
    return "uniform";
  case KernelFamily::Stencil:
    return "stencil";
  case KernelFamily::Wavefront:
    return "wavefront";
  case KernelFamily::ReductionMixed:
    return "reduction_mixed";
  case KernelFamily::TimestepChain:
    return "timestep_chain";
  case KernelFamily::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<KernelFamily> mlir::arts::parseKernelFamily(StringRef str) {
  if (str == "uniform")
    return KernelFamily::Uniform;
  if (str == "stencil")
    return KernelFamily::Stencil;
  if (str == "wavefront")
    return KernelFamily::Wavefront;
  if (str == "reduction_mixed")
    return KernelFamily::ReductionMixed;
  if (str == "timestep_chain")
    return KernelFamily::TimestepChain;
  if (str == "unknown")
    return KernelFamily::Unknown;
  return std::nullopt;
}

StringRef mlir::arts::iterationTopologyToString(IterationTopology topo) {
  switch (topo) {
  case IterationTopology::Serial:
    return "serial";
  case IterationTopology::OwnerStrip:
    return "owner_strip";
  case IterationTopology::OwnerTile:
    return "owner_tile";
  case IterationTopology::WavefrontTopo:
    return "wavefront";
  case IterationTopology::PersistentStrip:
    return "persistent_strip";
  case IterationTopology::PersistentTile:
    return "persistent_tile";
  }
  return "serial";
}

std::optional<IterationTopology>
mlir::arts::parseIterationTopology(StringRef str) {
  if (str == "serial")
    return IterationTopology::Serial;
  if (str == "owner_strip")
    return IterationTopology::OwnerStrip;
  if (str == "owner_tile")
    return IterationTopology::OwnerTile;
  if (str == "wavefront")
    return IterationTopology::WavefrontTopo;
  if (str == "persistent_strip")
    return IterationTopology::PersistentStrip;
  if (str == "persistent_tile")
    return IterationTopology::PersistentTile;
  return std::nullopt;
}

StringRef mlir::arts::asyncStrategyToString(AsyncStrategy strategy) {
  switch (strategy) {
  case AsyncStrategy::Blocking:
    return "blocking";
  case AsyncStrategy::AdvanceEdt:
    return "advance_edt";
  case AsyncStrategy::CpsChain:
    return "cps_chain";
  case AsyncStrategy::PersistentStructuredRegion:
    return "persistent_structured_region";
  }
  return "blocking";
}

std::optional<AsyncStrategy> mlir::arts::parseAsyncStrategy(StringRef str) {
  if (str == "blocking")
    return AsyncStrategy::Blocking;
  if (str == "advance_edt")
    return AsyncStrategy::AdvanceEdt;
  if (str == "cps_chain")
    return AsyncStrategy::CpsChain;
  if (str == "persistent_structured_region")
    return AsyncStrategy::PersistentStructuredRegion;
  return std::nullopt;
}

StringRef mlir::arts::repetitionStructureToString(RepetitionStructure rep) {
  switch (rep) {
  case RepetitionStructure::None:
    return "none";
  case RepetitionStructure::PairStep:
    return "pair_step";
  case RepetitionStructure::KStep:
    return "k_step";
  case RepetitionStructure::FullTimestep:
    return "full_timestep";
  }
  return "none";
}

std::optional<RepetitionStructure>
mlir::arts::parseRepetitionStructure(StringRef str) {
  if (str == "none")
    return RepetitionStructure::None;
  if (str == "pair_step")
    return RepetitionStructure::PairStep;
  if (str == "k_step")
    return RepetitionStructure::KStep;
  if (str == "full_timestep")
    return RepetitionStructure::FullTimestep;
  return std::nullopt;
}

///===----------------------------------------------------------------------===///
/// StructuredKernelPlanAnalysis
///===----------------------------------------------------------------------===///

StructuredKernelPlanAnalysis::StructuredKernelPlanAnalysis(AnalysisManager &AM)
    : ArtsAnalysis(AM) {}

void StructuredKernelPlanAnalysis::invalidate() {
  forOpPlans.clear();
  edtOpPlans.clear();
  hasRun = false;
}

void StructuredKernelPlanAnalysis::run(ModuleOp module) {
  if (hasRun)
    return;
  hasRun = true;

  module.walk([&](EdtOp edt) {
    if (edt.getType() != EdtType::parallel && edt.getType() != EdtType::task)
      return;

    edt.walk([&](ForOp forOp) {
      if (forOp->getParentOfType<EdtOp>() != edt)
        return;

      if (auto plan = synthesizePlan(forOp, edt)) {
        ARTS_DEBUG("Synthesized plan for ForOp: family="
                   << kernelFamilyToString(plan->family) << " topology="
                   << iterationTopologyToString(plan->topology));
        forOpPlans[forOp.getOperation()] = *plan;
        /// Also store on the parent EDT (use the first valid plan).
        if (!edtOpPlans.count(edt.getOperation()))
          edtOpPlans[edt.getOperation()] = *plan;
      }
    });
  });

  ARTS_DEBUG("Plan synthesis complete: "
             << forOpPlans.size() << " ForOp plans, " << edtOpPlans.size()
             << " EdtOp plans");
}

const StructuredKernelPlan *
StructuredKernelPlanAnalysis::getPlan(ForOp forOp) const {
  auto it = forOpPlans.find(forOp.getOperation());
  if (it != forOpPlans.end())
    return &it->second;
  return nullptr;
}

const StructuredKernelPlan *
StructuredKernelPlanAnalysis::getPlan(EdtOp edtOp) const {
  auto it = edtOpPlans.find(edtOp.getOperation());
  if (it != edtOpPlans.end())
    return &it->second;
  return nullptr;
}

///===----------------------------------------------------------------------===///
/// Plan synthesis
///===----------------------------------------------------------------------===///

std::optional<StructuredKernelPlan>
StructuredKernelPlanAnalysis::synthesizePlan(ForOp forOp, EdtOp parentEdt) {
  /// 1. Classify kernel family.
  KernelFamily family = classifyFamily(forOp, parentEdt);
  if (family == KernelFamily::Unknown)
    return std::nullopt;

  StructuredKernelPlan plan;
  plan.family = family;

  /// 2. Read ownerDims from stencil attrs or contract.
  if (auto dims = getStencilOwnerDims(forOp.getOperation()))
    plan.ownerDims = *dims;
  else if (auto dims = getStencilOwnerDims(parentEdt.getOperation()))
    plan.ownerDims = *dims;

  /// 3. Read blockShape from stencil attrs.
  if (auto shape = getStencilBlockShape(forOp.getOperation()))
    plan.physicalBlockShape = *shape;
  else if (auto shape = getStencilBlockShape(parentEdt.getOperation()))
    plan.physicalBlockShape = *shape;

  /// 4. Read haloShape from stencil min/max offsets.
  if (auto minOffs = getStencilMinOffsets(forOp.getOperation())) {
    if (auto maxOffs = getStencilMaxOffsets(forOp.getOperation())) {
      plan.haloShape.clear();
      for (size_t i = 0; i < minOffs->size() && i < maxOffs->size(); ++i)
        plan.haloShape.push_back((*maxOffs)[i] - (*minOffs)[i]);
    }
  } else if (auto minOffs = getStencilMinOffsets(parentEdt.getOperation())) {
    if (auto maxOffs = getStencilMaxOffsets(parentEdt.getOperation())) {
      plan.haloShape.clear();
      for (size_t i = 0; i < minOffs->size() && i < maxOffs->size(); ++i)
        plan.haloShape.push_back((*maxOffs)[i] - (*minOffs)[i]);
    }
  }

  /// 5. Determine if internode.
  bool isInternode = parentEdt.getConcurrency() == EdtConcurrency::internode;

  /// 6. Select topology.
  plan.topology = selectTopology(family, isInternode);

  /// 7. Detect repetition structure.
  plan.repetition = detectRepetition(forOp);

  /// 8. Select async strategy.
  plan.asyncStrategy = selectAsyncStrategy(plan.repetition);

  /// 9. Compute cost signals.
  plan.costSignals = computeCostSignals(plan, forOp);

  return plan;
}

KernelFamily
StructuredKernelPlanAnalysis::classifyFamily(ForOp forOp,
                                             EdtOp parentEdt) const {
  /// Read dep pattern from ForOp or parent EDT.
  auto depPattern = getEffectiveDepPattern(forOp.getOperation());
  if (!depPattern)
    return KernelFamily::Unknown;

  /// Classify based on dep pattern family.
  if (isStencilFamilyDepPattern(*depPattern)) {
    /// Distinguish wavefront from regular stencil.
    if (*depPattern == ArtsDepPattern::wavefront_2d)
      return KernelFamily::Wavefront;
    return KernelFamily::Stencil;
  }

  if (isUniformFamilyDepPattern(*depPattern))
    return KernelFamily::Uniform;

  if (*depPattern == ArtsDepPattern::matmul)
    return KernelFamily::ReductionMixed;

  if (*depPattern == ArtsDepPattern::triangular)
    return KernelFamily::Unknown; /// Irregular, no plan

  return KernelFamily::Unknown;
}

IterationTopology
StructuredKernelPlanAnalysis::selectTopology(KernelFamily family,
                                             bool isInternode) const {
  switch (family) {
  case KernelFamily::Uniform:
  case KernelFamily::ReductionMixed:
    return isInternode ? IterationTopology::OwnerTile
                       : IterationTopology::OwnerStrip;
  case KernelFamily::Stencil:
  case KernelFamily::TimestepChain:
    return IterationTopology::OwnerStrip;
  case KernelFamily::Wavefront:
    return IterationTopology::WavefrontTopo;
  case KernelFamily::Unknown:
    return IterationTopology::Serial;
  }
  return IterationTopology::Serial;
}

AsyncStrategy StructuredKernelPlanAnalysis::selectAsyncStrategy(
    RepetitionStructure repetition) const {
  switch (repetition) {
  case RepetitionStructure::None:
    return AsyncStrategy::Blocking;
  case RepetitionStructure::PairStep:
  case RepetitionStructure::KStep:
    return AsyncStrategy::AdvanceEdt;
  case RepetitionStructure::FullTimestep:
    return AsyncStrategy::CpsChain;
  }
  return AsyncStrategy::Blocking;
}

RepetitionStructure
StructuredKernelPlanAnalysis::detectRepetition(ForOp forOp) const {
  /// Walk up to find enclosing scf.for that looks like a timestep loop.
  Operation *parent = forOp->getParentOp();
  while (parent) {
    if (auto scfFor = dyn_cast<scf::ForOp>(parent)) {
      /// A constant-bound enclosing loop suggests repetition.
      if (auto lb = scfFor.getLowerBound().getDefiningOp<arith::ConstantOp>())
        if (auto ub = scfFor.getUpperBound().getDefiningOp<arith::ConstantOp>())
          return RepetitionStructure::FullTimestep;
    }
    parent = parent->getParentOp();
  }
  return RepetitionStructure::None;
}

PlanCostSignals StructuredKernelPlanAnalysis::computeCostSignals(
    const StructuredKernelPlan &plan, ForOp forOp) const {
  PlanCostSignals signals;

  /// Scheduler overhead: higher for more complex topologies.
  switch (plan.topology) {
  case IterationTopology::Serial:
    signals.schedulerOverhead = 0.0;
    break;
  case IterationTopology::OwnerStrip:
    signals.schedulerOverhead = 0.3;
    break;
  case IterationTopology::OwnerTile:
    signals.schedulerOverhead = 0.5;
    break;
  case IterationTopology::WavefrontTopo:
    signals.schedulerOverhead = 0.7;
    break;
  case IterationTopology::PersistentStrip:
  case IterationTopology::PersistentTile:
    signals.schedulerOverhead = 0.1;
    break;
  }

  /// Slice widening pressure from halo.
  if (!plan.haloShape.empty()) {
    double totalHalo = 0.0;
    for (int64_t h : plan.haloShape)
      totalHalo += static_cast<double>(h);
    signals.sliceWideningPressure = totalHalo;
  }

  /// Repetition amortization benefit.
  switch (plan.repetition) {
  case RepetitionStructure::None:
    signals.relaunchAmortization = 0.0;
    break;
  case RepetitionStructure::PairStep:
    signals.relaunchAmortization = 0.5;
    break;
  case RepetitionStructure::KStep:
    signals.relaunchAmortization = 0.7;
    break;
  case RepetitionStructure::FullTimestep:
    signals.relaunchAmortization = 1.0;
    break;
  }

  if (plan.repetition != RepetitionStructure::None)
    signals.expectedLocalWork = estimateExpectedLocalWork(forOp);

  return signals;
}
