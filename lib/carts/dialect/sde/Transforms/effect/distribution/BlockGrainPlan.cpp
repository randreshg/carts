///==========================================================================///
/// File: BlockGrainPlan.cpp
///
/// SDE block-grain planning: budget-reconciled block grain commit for
/// multi-owner data-parallel writers plus A1 same-owner grain unification
/// (reconcileSameOwnerArrayGrain). Logic carved verbatim from the
/// correctness-base @782988ad1 DistributionPlanning pass. The v4 @0b9338f07
/// simplified `commitBudgetReconciledLayout` and the removal of
/// `reconcileSameOwnerArrayGrain` are deliberately NOT used here.
///==========================================================================///

#include "carts/dialect/sde/Transforms/Passes.h"
namespace mlir::carts::sde {
#define GEN_PASS_DEF_BLOCKGRAINPLAN
#include "carts/dialect/sde/Transforms/Passes.h.inc"
} // namespace mlir::carts::sde

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/dialect/sde/Transforms/effect/distribution/BlockGrainPlan.h"
#include "carts/dialect/sde/Transforms/effect/distribution/DistributionLayoutUtils.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SDECostModel.h"
#include "carts/dialect/sde/Utils/SdeAttrNames.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <numeric>
#include <optional>

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde::distribution;

namespace {

struct BlockGrainPlan {
  SmallVector<int64_t, 4> ownerDims;
  SmallVector<int64_t, 4> physicalBlockShape;
  SmallVector<int64_t, 4> logicalWorkerSlice;
  SmallVector<int64_t, 4> haloShape;
};

static int64_t unifyBlockDim(int64_t lhs, int64_t rhs,
                             SameOwnerGrainUnifyKind unifyKind) {
  if (lhs <= 0)
    return rhs;
  if (rhs <= 0)
    return lhs;
  if (lhs == rhs)
    return lhs;
  if (unifyKind == SameOwnerGrainUnifyKind::CoarseCompatiblePostExpand) {
    int64_t lo = std::min(lhs, rhs);
    int64_t hi = std::max(lhs, rhs);
    if (hi % lo == 0)
      return hi;
  }
  int64_t g = std::gcd(lhs, rhs);
  return g > 0 ? g : std::min(lhs, rhs);
}

static void unifyBlockShape(ArrayRef<int64_t> grain,
                            SmallVector<int64_t, 4> &unified,
                            SameOwnerGrainUnifyKind unifyKind) {
  if (grain.empty())
    return;
  if (unified.empty()) {
    unified.assign(grain.begin(), grain.end());
    return;
  }
  if (unified.size() != grain.size())
    return;
  for (size_t i = 0; i < grain.size(); ++i)
    unified[i] = unifyBlockDim(unified[i], grain[i], unifyKind);
}

static bool mergeCompatibleBudgetGrain(ArrayRef<int64_t> budget,
                                       SmallVectorImpl<int64_t> &selected) {
  if (budget.empty())
    return false;
  if (selected.empty()) {
    selected.assign(budget.begin(), budget.end());
    return true;
  }
  if (selected.size() != budget.size())
    return false;
  for (auto [idx, dim] : llvm::enumerate(budget)) {
    int64_t current = selected[idx];
    int64_t lo = std::min(current, dim);
    int64_t hi = std::max(current, dim);
    if (lo <= 0 || hi % lo != 0)
      return false;
  }
  for (auto [idx, dim] : llvm::enumerate(budget))
    selected[idx] = std::min<int64_t>(selected[idx], dim);
  return true;
}

static std::optional<BlockGrainPlan>
buildCrossArrayBudgetBlockGrainPlan(sde::SdeSuIterateOp op,
                                    sde::SDECostModel &costModel) {
  if (!op)
    return std::nullopt;
  if (auto cls = sde::queryStructuredClassification(op);
      cls && *cls == sde::SdeStructuredClassification::stencil)
    return std::nullopt;
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return std::nullopt;

  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan || outputPlan->shape.empty())
    return std::nullopt;

  bool sawWrite = false;
  bool sawBudget = false;
  bool sawFinerBudget = false;
  SmallVector<int64_t, 4> layoutOwnerDims;
  SmallVector<int64_t, 4> selectedBudget;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.layoutKind != sde::ArrayLayoutKind::blockParallel ||
        fact.ownerDims.empty() || fact.blockShape.empty())
      continue;
    if (layoutOwnerDims.empty()) {
      layoutOwnerDims.assign(fact.ownerDims.begin(), fact.ownerDims.end());
    } else if (ArrayRef<int64_t>(layoutOwnerDims) !=
               ArrayRef<int64_t>(fact.ownerDims)) {
      return std::nullopt;
    }
    sawWrite |= fact.role == sde::LayoutGraphRole::write;
    if (fact.budgetBlockShape.empty())
      continue;
    if (fact.budgetBlockShape.size() != outputPlan->shape.size())
      return std::nullopt;
    if (!mergeCompatibleBudgetGrain(fact.budgetBlockShape, selectedBudget))
      return std::nullopt;
    sawBudget = true;
    sawFinerBudget |= fact.budgetBlockShape != fact.blockShape;
  }
  if (!sawWrite || !sawBudget || !sawFinerBudget || selectedBudget.empty())
    return std::nullopt;

  std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
      orderPhysicalOwnerDimsByLoop(*outputPlan, layoutOwnerDims,
                                   op.getLowerBounds().size());
  if (!orderedOwnerDims || orderedOwnerDims->empty())
    return std::nullopt;
  if (!allExternalStoresCoverOwnerDims(op, *orderedOwnerDims,
                                       outputPlan->physicalDimToLoopDim))
    return std::nullopt;

  for (auto [idx, dim] : llvm::enumerate(selectedBudget))
    if (dim <= 0 || outputPlan->shape[idx] <= 0 || dim > outputPlan->shape[idx])
      return std::nullopt;

  BlockGrainPlan plan;
  plan.ownerDims.assign(orderedOwnerDims->begin(), orderedOwnerDims->end());
  plan.physicalBlockShape.assign(selectedBudget.begin(), selectedBudget.end());

  bool anyHalo = false;
  for (int64_t physicalDim : plan.ownerDims) {
    int64_t loopDim = physicalDim;
    if (physicalDim >= 0 &&
        static_cast<size_t>(physicalDim) <
            outputPlan->physicalDimToLoopDim.size() &&
        outputPlan->physicalDimToLoopDim[physicalDim] >= 0)
      loopDim = outputPlan->physicalDimToLoopDim[physicalDim];
    int64_t halo =
        loopDim >= 0
            ? readStencilHaloForOwnerDim(op, static_cast<unsigned>(loopDim))
            : 0;
    plan.haloShape.push_back(std::max<int64_t>(0, halo));
    anyHalo |= halo > 0;
  }
  if (!anyHalo)
    plan.haloShape.clear();

  plan.logicalWorkerSlice.assign(plan.physicalBlockShape.begin(),
                                 plan.physicalBlockShape.end());
  auto classification = sde::queryStructuredClassification(op);
  bool isStencil = classification &&
                   *classification == sde::SdeStructuredClassification::stencil;
  if (isStencil || anyHalo) {
    if (!sde::buildBlockAlignedLogicalWorkerSlice(
            outputPlan->shape, plan.ownerDims, plan.physicalBlockShape,
            costModel.getLogicalWorkerCapacity(), plan.logicalWorkerSlice))
      plan.logicalWorkerSlice.assign(plan.physicalBlockShape.begin(),
                                     plan.physicalBlockShape.end());
  } else {
    plan.logicalWorkerSlice = buildLogicalWorkerSliceOrPhysical(
        op, outputPlan->shape, plan.ownerDims, plan.physicalBlockShape,
        costModel.getLogicalWorkerCapacity(),
        anyHalo ? ArrayRef<int64_t>(plan.haloShape) : ArrayRef<int64_t>{});
  }
  return plan;
}

// Choose the one committed node-agnostic budget grain for every SU that writes
// a multi-owner-distributed data-parallel array. The caller only commits it
// when the current SU step already realizes the selected block grain.
static std::optional<BlockGrainPlan>
buildBudgetReconciledBlockGrainPlan(sde::SdeSuIterateOp op,
                                    sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
    return std::nullopt;
  // Matmul/contraction keeps its dedicated contraction-tiling plan: its CU-task
  // grain is the reduction-aware worker grain, not the data-parallel block
  // grain reconciled here. This is the one genuinely layout-irreducible family.
  if (auto cls = sde::queryStructuredClassification(op);
      cls && *cls == sde::SdeStructuredClassification::matmul)
    return std::nullopt;
  if (auto cls = sde::queryStructuredClassification(op);
      cls && *cls == sde::SdeStructuredClassification::stencil)
    return std::nullopt;
  if (auto cls = sde::queryStructuredClassification(op);
      cls &&
      (*cls == sde::SdeStructuredClassification::elementwise ||
       *cls == sde::SdeStructuredClassification::elementwise_pipeline) &&
      sde::queryInPlaceSafe(op))
    return std::nullopt;
  if (auto pat = sde::querySuPattern(op);
      pat && *pat == sde::SdePattern::matmul)
    return std::nullopt;
  std::optional<sde::LayoutGraphFact> writeLayout =
      selectSingleWriteLayoutFact(op);
  if (!writeLayout || writeLayout->ownerDims.size() < 2 ||
      writeLayout->budgetBlockShape.empty())
    return std::nullopt;
  // owner_tile needs one realized SDE loop dimension per owner dim. A 1-D loop
  // (e.g. a residual/reduction loop over the same array) that did not get
  // promoted cannot carry a multi-owner tile; leave it to the pattern
  // committers rather than committing unverifiable owner_tile facts.
  if (op.getLowerBounds().size() < writeLayout->ownerDims.size())
    return std::nullopt;
  std::optional<sde::SuOutputLayoutFacts> outputPlan =
      sde::findCompatibleSuOutputLayoutFacts(op);
  if (!outputPlan)
    return std::nullopt;
  std::optional<SmallVector<int64_t, 4>> orderedOwnerDims =
      orderPhysicalOwnerDimsByLoop(*outputPlan, writeLayout->ownerDims,
                                   op.getLowerBounds().size());
  if (!orderedOwnerDims)
    return std::nullopt;
  BlockGrainPlan plan;
  plan.ownerDims.assign(orderedOwnerDims->begin(), orderedOwnerDims->end());
  if (!allExternalStoresCoverOwnerDims(op, plan.ownerDims,
                                       outputPlan->physicalDimToLoopDim))
    return std::nullopt;
  plan.physicalBlockShape.assign(writeLayout->budgetBlockShape.begin(),
                                 writeLayout->budgetBlockShape.end());
  if (!sde::enforceOwnerBlockConcurrencyFloor(
          outputPlan->shape, plan.ownerDims,
          getInterLocalityTargetWorkers(costModel), plan.physicalBlockShape))
    return std::nullopt;
  // Per-owner-dim halo from the op's stencil access offsets (0 for
  // non-stencils). Not compared by hasSameHostBridgePlan, but needed for
  // correct halo exchange.
  bool anyHalo = false;
  for (int64_t od : plan.ownerDims) {
    int64_t h =
        od >= 0 ? readStencilHaloForOwnerDim(op, static_cast<unsigned>(od)) : 0;
    plan.haloShape.push_back(std::max<int64_t>(0, h));
    anyHalo |= h > 0;
  }
  plan.logicalWorkerSlice = buildLogicalWorkerSliceOrPhysical(
      op, outputPlan->shape, plan.ownerDims, plan.physicalBlockShape,
      costModel.getLogicalWorkerCapacity(),
      anyHalo ? ArrayRef<int64_t>(plan.haloShape) : ArrayRef<int64_t>{});
  if (!anyHalo)
    plan.haloShape.clear();
  return plan;
}

} // namespace

namespace mlir::carts::sde::distribution {

bool commitBudgetReconciledLayout(sde::SdeSuIterateOp op,
                                  sde::SDECostModel &costModel) {
  if (!op || hasCommittedPhysicalLayout(op))
    return false;

  std::optional<BlockGrainPlan> plan =
      buildBudgetReconciledBlockGrainPlan(op, costModel);
  if (plan)
    return applyPhysicalLayoutIfRealized(
        op, plan->ownerDims, plan->physicalBlockShape, plan->haloShape,
        plan->logicalWorkerSlice);

  plan = buildCrossArrayBudgetBlockGrainPlan(op, costModel);
  if (!plan)
    return false;
  return applyPhysicalLayoutIfRealized(
      op, plan->ownerDims, plan->physicalBlockShape, plan->haloShape,
      plan->logicalWorkerSlice);
}

// A1 grain unification. Producer and consumer SUs of the same distributed array
// can commit different block grains for purely scheduling reasons (e.g. a
// matmul writer at [512,*] and its consumer at budget [256,*]).
// RedistributionEdges then reads a same-owner re-tile and fails closed
// ("refusing to emit degenerate all_to_all") because no movement op realizes a
// within-owner re-block. The grain is a planning artifact, not a data-location
// difference: commit ONE grain (the per-dim GCD, finest both already realize)
// on every block_parallel fact of the array so home==reader and no
// redistribution edge exists. Node-agnostic; runs after per-SU commit so
// RankExpandMu/RedistributionEdges consume unified facts.
void reconcileSameOwnerArrayGrain(Operation *moduleOp,
                                  SameOwnerGrainUnifyKind unifyKind) {
  auto committedGrain = [](const sde::LayoutGraphFact &f) -> ArrayRef<int64_t> {
    return f.budgetBlockShape.empty() ? ArrayRef<int64_t>(f.blockShape)
                                      : ArrayRef<int64_t>(f.budgetBlockShape);
  };
  struct GrainInfo {
    SmallVector<int64_t, 4> ownerDims;
    SmallVector<int64_t, 4> unified;
    SmallVector<int64_t, 4> rootShape;
    SmallVector<sde::SdeSuIterateOp, 2> writerOps;
    bool eligible = true;
    bool seen = false;
    bool differs = false;
    // Per-dim GCD of the genuine node-agnostic *budget* grains. A writer that
    // committed only an abstract coarse blockShape (no budgetBlockShape) must
    // be reconciled DOWN to this budget rather than dragging the unified grain
    // to a coprime GCD with it: gcd(abstractBlock, budget) can be 1, which is a
    // grain NEITHER side realizes (one DB block per element -> millions of
    // EDTs). The budget is the one grain both sides can realize.
    SmallVector<int64_t, 4> budgetUnified;
    bool budgetSeen = false;
    bool budgetSizeMismatch = false;
    // Set when some contributing grain is *incompatible* with the budget grain
    // on a dim (neither divides the other), so the cross-fact GCD collapses to
    // a grain the incompatible side cannot realize. A grain that merely divides
    // the budget (a legitimate finer matmul/elementwise tile) is compatible and
    // is preserved.
    bool incompatibleWithBudget = false;
    // Every per-dim grain that contributed to the cross-fact GCD, for the
    // post-walk budget-compatibility check.
    SmallVector<SmallVector<int64_t, 4>, 4> contributingGrains;
  };
  llvm::DenseMap<int64_t, GrainInfo> byId;

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    for (const sde::LayoutGraphFact &f : sde::parseArrayLayoutFacts(layout)) {
      if (f.id < 0)
        continue;
      GrainInfo &info = byId[f.id];
      if (f.layoutKind != sde::ArrayLayoutKind::blockParallel ||
          f.ownerDims.empty()) {
        info.eligible = false;
        continue;
      }
      ArrayRef<int64_t> grain = committedGrain(f);
      if (grain.empty()) {
        info.eligible = false;
        continue;
      }
      info.contributingGrains.push_back(
          SmallVector<int64_t, 4>(grain.begin(), grain.end()));
      if (!info.seen) {
        info.seen = true;
        info.ownerDims.assign(f.ownerDims.begin(), f.ownerDims.end());
        info.unified.assign(grain.begin(), grain.end());
      } else if (ArrayRef<int64_t>(info.ownerDims) !=
                     ArrayRef<int64_t>(f.ownerDims) ||
                 info.unified.size() != grain.size()) {
        info.eligible = false;
      } else {
        for (size_t i = 0; i < grain.size(); ++i) {
          if (grain[i] != info.unified[i])
            info.differs = true;
        }
        unifyBlockShape(grain, info.unified, unifyKind);
      }
      // Track the GCD of declared budget grains separately. Only facts that
      // actually carry a budgetBlockShape contribute, so a coarse abstract
      // writer blockShape never collapses this.
      if (!f.budgetBlockShape.empty()) {
        ArrayRef<int64_t> budget(f.budgetBlockShape);
        if (!info.budgetSeen) {
          info.budgetSeen = true;
          info.budgetUnified.assign(budget.begin(), budget.end());
        } else if (info.budgetUnified.size() != budget.size()) {
          info.budgetSizeMismatch = true;
        } else {
          for (size_t i = 0; i < budget.size(); ++i) {
            int64_t g = std::gcd(info.budgetUnified[i], budget[i]);
            info.budgetUnified[i] = g > 0 ? g : info.budgetUnified[i];
          }
        }
      }
      if (f.role == sde::LayoutGraphRole::write) {
        info.writerOps.push_back(op);
        if (info.rootShape.empty())
          if (auto rs = sde::findWriteArrayRootShape(op, f.id))
            info.rootShape.assign(rs->begin(), rs->end());
      }
    }
  });

  // When a declared budget grain exists, it is the authoritative node-agnostic
  // grain both producers and consumers can realize. The raw cross-fact GCD is
  // only safe when every contributing grain is *compatible* with the budget
  // (one divides the other on every dim). If some contributing grain is
  // incompatible with the budget (neither divides the other, e.g. a writer's
  // coarse abstract blockShape that was never physically committed at budget),
  // the GCD collapses to a grain that incompatible side cannot realize — for a
  // 1-D elementwise array this is one DB block per element (millions of EDTs).
  // In that case adopt the budget grain and re-commit the writers at it.
  for (auto &kv : byId) {
    GrainInfo &info = kv.second;
    if (!info.eligible || !info.seen || !info.budgetSeen ||
        info.budgetSizeMismatch)
      continue;
    if (info.unified.size() != info.budgetUnified.size())
      continue;
    for (const SmallVector<int64_t, 4> &grain : info.contributingGrains) {
      if (grain.size() != info.budgetUnified.size())
        continue;
      for (size_t i = 0; i < grain.size(); ++i) {
        int64_t g = grain[i], b = info.budgetUnified[i];
        if (g <= 0 || b <= 0)
          continue;
        int64_t common = std::gcd(g, b);
        if (common != std::min(g, b))
          info.incompatibleWithBudget = true;
      }
    }
    if (!info.incompatibleWithBudget)
      continue;
    // Adopt the budget grain as the unified target and force the differing
    // writers to be re-committed at it.
    info.unified.assign(info.budgetUnified.begin(), info.budgetUnified.end());
    info.differs = true;
  }

  llvm::DenseMap<int64_t, SmallVector<int64_t, 4>> targetById;
  for (auto &kv : byId) {
    GrainInfo &info = kv.second;
    if (info.eligible && info.seen && info.differs && !info.ownerDims.empty())
      targetById[kv.first] = info.unified;
  }
  if (targetById.empty())
    return;

  for (auto &kv : targetById) {
    GrainInfo &info = byId[kv.first];
    for (sde::SdeSuIterateOp writer : info.writerOps)
      if (writer)
        (void)sde::rewriteWriterArrayLayoutToPhysicalShape(
            writer, info.ownerDims, kv.second, kv.first);
  }

  for (auto &kv : byId) {
    GrainInfo &info = kv.second;
    if (!info.incompatibleWithBudget || !info.eligible || !info.seen)
      continue;
    for (sde::SdeSuIterateOp writer : info.writerOps)
      if (writer)
        (void)sde::commitWriterPhysicalLayoutFacts(writer, info.ownerDims,
                                                   targetById[kv.first]);
  }

  MLIRContext *ctx = moduleOp->getContext();
  Builder builder(ctx);
  StringAttr blockShapeName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BlockShape);
  StringAttr budgetName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::BudgetBlockShape);
  StringAttr muCountName =
      builder.getStringAttr(sde::AttrNames::LayoutGraph::MuBlockCount);

  moduleOp->walk([&](sde::SdeSuIterateOp op) {
    ArrayAttr layout = op.getArrayLayoutAttr();
    if (!layout)
      return;
    bool changed = false;
    SmallVector<Attribute, 4> rewritten;
    rewritten.reserve(layout.size());
    for (Attribute attr : layout) {
      auto dict = dyn_cast<DictionaryAttr>(attr);
      std::optional<sde::LayoutGraphFact> f =
          dict ? sde::parseArrayLayoutFact(dict) : std::nullopt;
      if (!dict || !f) {
        rewritten.push_back(attr);
        continue;
      }
      auto it = targetById.find(f->id);
      const GrainInfo &info = byId[f->id];
      if (it == targetById.end() ||
          f->layoutKind != sde::ArrayLayoutKind::blockParallel ||
          ArrayRef<int64_t>(f->ownerDims) !=
              ArrayRef<int64_t>(info.ownerDims) ||
          it->second.size() != committedGrain(*f).size() ||
          committedGrain(*f) == ArrayRef<int64_t>(it->second)) {
        rewritten.push_back(attr);
        continue;
      }
      ArrayRef<int64_t> target = it->second;
      SmallVector<NamedAttribute, 8> fields;
      for (NamedAttribute named : dict)
        if (named.getName() != blockShapeName &&
            named.getName() != budgetName && named.getName() != muCountName)
          fields.push_back(named);
      fields.push_back(
          builder.getNamedAttr(blockShapeName, buildI64ArrayAttr(ctx, target)));
      fields.push_back(
          builder.getNamedAttr(budgetName, buildI64ArrayAttr(ctx, target)));
      if (!info.rootShape.empty() && info.rootShape.size() == target.size()) {
        int64_t count = sde::inferCuCountFromMuPartition(
            info.rootShape, info.ownerDims, target);
        if (count > 0)
          fields.push_back(builder.getNamedAttr(
              muCountName, builder.getI64IntegerAttr(count)));
      }
      rewritten.push_back(builder.getDictionaryAttr(fields));
      changed = true;
    }
    if (changed)
      op.setArrayLayoutAttr(ArrayAttr::get(ctx, rewritten));
    sde::syncSuTypedArrayLayoutFacts(op);
  });
}

} // namespace mlir::carts::sde::distribution

namespace {

struct BlockGrainPlanPass
    : public sde::impl::BlockGrainPlanBase<BlockGrainPlanPass> {
  explicit BlockGrainPlanPass(sde::SDECostModel *costModel = nullptr)
      : costModel(costModel) {}

  void runOnOperation() override {
    if (!costModel)
      return;
    // Unify producer/consumer block grain per array so a same-owner re-tile is
    // never left for RedistributionEdges to reject (A1). Runs after all per-SU
    // facts are committed; downstream passes consume the unified facts.
    reconcileSameOwnerArrayGrain(getOperation());
  }

private:
  sde::SDECostModel *costModel = nullptr;
};

} // namespace

namespace mlir::carts::sde {

std::unique_ptr<Pass> createBlockGrainPlanPass(sde::SDECostModel *costModel) {
  return std::make_unique<BlockGrainPlanPass>(costModel);
}

} // namespace mlir::carts::sde
