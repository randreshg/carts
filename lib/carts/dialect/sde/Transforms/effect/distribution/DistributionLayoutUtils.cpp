///==========================================================================///
/// File: DistributionLayoutUtils.cpp
///
/// Shared layout/commit helpers for the SDE distribution passes. Extracted
/// verbatim from the correctness-base @782988ad1 DistributionPlanning pass.
///==========================================================================///

#include "carts/dialect/sde/Transforms/effect/distribution/DistributionLayoutUtils.h"

#include "carts/dialect/sde/Analysis/LayoutGraph.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Utils/IterationSizingUtils.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ValueAnalysis.h"

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

#include <algorithm>
#include <limits>

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde::distribution {

int64_t saturatingMultiplyPositive(int64_t lhs, int64_t rhs) {
  lhs = std::max<int64_t>(1, lhs);
  rhs = std::max<int64_t>(1, rhs);
  if (lhs > std::numeric_limits<int64_t>::max() / rhs)
    return std::numeric_limits<int64_t>::max();
  return lhs * rhs;
}

int64_t getInterLocalityTargetWorkers(sde::SDECostModel &costModel) {
  return saturatingMultiplyPositive(costModel.getLogicalWorkerCapacity(),
                                    costModel.getInterLocalityTaskWaves());
}

int64_t readStencilHaloForOwnerDim(sde::SdeSuIterateOp op, unsigned ownerDim) {
  std::optional<sde::SuNeighborhoodAccessInfo> neighborhood =
      sde::queryNeighborhoodAccessInfo(op);
  if (!neighborhood)
    return 0;

  for (auto [idx, rawDim] : llvm::enumerate(neighborhood->ownerDims)) {
    if (rawDim < 0 || static_cast<unsigned>(rawDim) != ownerDim)
      continue;
    if (idx >= neighborhood->minOffsets.size() ||
        idx >= neighborhood->maxOffsets.size())
      return 0;
    return std::max<int64_t>(0, std::max(-neighborhood->minOffsets[idx],
                                         neighborhood->maxOffsets[idx]));
  }
  return 0;
}

std::optional<sde::LayoutGraphFact>
layoutFactFromCommittedPhysicalLayout(sde::SdeSuIterateOp op) {
  std::optional<sde::CommittedSuPhysicalLayout> layout =
      sde::recoverCommittedPhysicalLayout(op);
  if (!layout || layout->ownerDims.empty() || layout->blockShape.empty())
    return std::nullopt;
  sde::LayoutGraphFact fact;
  fact.role = sde::LayoutGraphRole::write;
  fact.layoutKind = sde::ArrayLayoutKind::blockParallel;
  fact.ownerDims = layout->ownerDims;
  fact.blockShape = layout->blockShape;
  return fact;
}

std::optional<sde::LayoutGraphFact>
selectSingleWriteLayoutFact(sde::SdeSuIterateOp op) {
  ArrayAttr layout = op.getArrayLayoutAttr();
  if (!layout)
    return layoutFactFromCommittedPhysicalLayout(op);

  std::optional<sde::LayoutGraphFact> selected;
  llvm::SmallDenseSet<int64_t, 4> writtenIds;
  for (const sde::LayoutGraphFact &fact : sde::parseArrayLayoutFacts(layout)) {
    if (fact.role != sde::LayoutGraphRole::write || fact.ownerDims.empty() ||
        fact.blockShape.empty())
      continue;
    if (fact.id < 0 || !writtenIds.insert(fact.id).second)
      return std::nullopt;
    if (!selected) {
      selected = fact;
      continue;
    }
    if (selected->layoutKind != fact.layoutKind ||
        selected->ownerDims != fact.ownerDims ||
        selected->blockShape != fact.blockShape ||
        selected->budgetBlockShape != fact.budgetBlockShape)
      return std::nullopt;
  }
  if (!selected)
    return layoutFactFromCommittedPhysicalLayout(op);
  return selected;
}

std::optional<unsigned> findDependentSuLoopSlot(Value index,
                                                ArrayRef<Value> loopIvs) {
  std::optional<unsigned> selected;
  for (auto [slot, iv] : llvm::enumerate(loopIvs)) {
    if (!sde::isOwnerDependentIndex(index, iv))
      continue;
    if (selected)
      return std::nullopt;
    selected = static_cast<unsigned>(slot);
  }
  return selected;
}

std::optional<SmallVector<int64_t, 4>>
derivePhysicalDimToSuLoopDimFromExternalStores(sde::SdeSuIterateOp op,
                                               ArrayRef<int64_t> ownerDims) {
  if (!op || ownerDims.empty() || op.getBody().empty())
    return std::nullopt;

  auto loopIvs = op.getLoopInductionVars();
  if (!loopIvs || loopIvs->empty())
    return std::nullopt;

  SmallVector<int64_t, 4> physicalDimToLoopDim;
  bool sawExternalStore = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType) {
      rejected = true;
      return;
    }
    if (memrefType.getRank() == 0)
      return;

    OperandRange indices = storeOp.getIndices();
    if (indices.empty()) {
      rejected = true;
      return;
    }
    if (physicalDimToLoopDim.empty())
      physicalDimToLoopDim.assign(indices.size(), -1);
    if (physicalDimToLoopDim.size() != indices.size()) {
      rejected = true;
      return;
    }

    sawExternalStore = true;
    for (int64_t ownerDim : ownerDims) {
      if (ownerDim < 0 || static_cast<size_t>(ownerDim) >= indices.size()) {
        rejected = true;
        return;
      }
      std::optional<unsigned> loopSlot =
          findDependentSuLoopSlot(indices[ownerDim], *loopIvs);
      if (!loopSlot) {
        rejected = true;
        return;
      }
      int64_t &mapped = physicalDimToLoopDim[ownerDim];
      if (mapped >= 0 && mapped != static_cast<int64_t>(*loopSlot)) {
        rejected = true;
        return;
      }
      mapped = static_cast<int64_t>(*loopSlot);
    }
  });

  if (rejected || !sawExternalStore)
    return std::nullopt;
  for (int64_t ownerDim : ownerDims)
    if (ownerDim < 0 ||
        static_cast<size_t>(ownerDim) >= physicalDimToLoopDim.size() ||
        physicalDimToLoopDim[ownerDim] < 0)
      return std::nullopt;
  return physicalDimToLoopDim;
}

bool hasCommittedPhysicalLayout(sde::SdeSuIterateOp op) {
  return sde::hasCommittedWriterBlockLayout(op);
}

bool isInPlaceSelfReadStencil(sde::SdeSuIterateOp op) {
  auto classification = sde::queryStructuredClassification(op);
  if (!classification ||
      *classification != sde::SdeStructuredClassification::stencil)
    return false;

  auto effects = sde::collectStructuredMemoryEffects(op.getBody());
  return !effects.hasUnknownEffects && sde::hasInPlaceSelfRead(effects);
}

void applyPhysicalPlan(sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
                       ArrayRef<int64_t> physicalBlockShape,
                       ArrayRef<int64_t> haloShape,
                       ArrayRef<int64_t> logicalWorkerSlice) {
  (void)haloShape;
  sde::commitWriterPhysicalLayoutFacts(op, ownerDims, physicalBlockShape,
                                       logicalWorkerSlice);
}

bool allowsGroupedLogicalWorkerSlice(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> haloShape) {
  if (llvm::any_of(haloShape, [](int64_t halo) { return halo > 0; }))
    return false;
  auto classification = sde::queryStructuredClassification(op);
  return !classification ||
         *classification != sde::SdeStructuredClassification::stencil;
}

SmallVector<int64_t, 4> buildLogicalWorkerSliceOrPhysical(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> shape,
    ArrayRef<int64_t> ownerDims, ArrayRef<int64_t> physicalBlockShape,
    int64_t targetComputeUnits, ArrayRef<int64_t> haloShape) {
  SmallVector<int64_t, 4> logicalWorkerSlice(physicalBlockShape.begin(),
                                             physicalBlockShape.end());
  if (!allowsGroupedLogicalWorkerSlice(op, haloShape))
    return logicalWorkerSlice;
  if (!sde::buildBlockAlignedLogicalWorkerSlice(
          shape, ownerDims, physicalBlockShape, targetComputeUnits,
          logicalWorkerSlice))
    logicalWorkerSlice.assign(physicalBlockShape.begin(),
                              physicalBlockShape.end());
  return logicalWorkerSlice;
}

bool physicalLayoutMatchesRealizedLoopSteps(
    sde::SdeSuIterateOp op, ArrayRef<int64_t> ownerDims,
    ArrayRef<int64_t> physicalBlockShape,
    ArrayRef<int64_t> logicalWorkerSlice) {
  if (!op || ownerDims.empty() || physicalBlockShape.empty() ||
      op.getSteps().empty())
    return false;

  std::optional<SmallVector<int64_t, 4>> physicalDimToLoopDim =
      derivePhysicalDimToSuLoopDimFromExternalStores(op, ownerDims);
  if (!physicalDimToLoopDim)
    return false;

  for (int64_t rawPhysicalDim : ownerDims) {
    if (rawPhysicalDim < 0 ||
        static_cast<size_t>(rawPhysicalDim) >= physicalBlockShape.size())
      return false;
    if (static_cast<size_t>(rawPhysicalDim) >= physicalDimToLoopDim->size())
      return false;
    int64_t loopDim = (*physicalDimToLoopDim)[rawPhysicalDim];
    if (loopDim < 0 || static_cast<size_t>(loopDim) >= op.getSteps().size())
      return false;

    std::optional<int64_t> realizedStep =
        ValueAnalysis::getPositiveConstantIndex(op.getSteps()[loopDim]);
    int64_t workerSpan = physicalBlockShape[rawPhysicalDim];
    if (!logicalWorkerSlice.empty()) {
      if (logicalWorkerSlice.size() != physicalBlockShape.size())
        return false;
      workerSpan = logicalWorkerSlice[rawPhysicalDim];
    }
    if (!realizedStep || workerSpan <= 0 ||
        physicalBlockShape[rawPhysicalDim] <= 0 ||
        workerSpan % physicalBlockShape[rawPhysicalDim] != 0 ||
        *realizedStep > workerSpan)
      return false;
  }
  return true;
}

bool applyPhysicalLayoutIfRealized(sde::SdeSuIterateOp op,
                                   ArrayRef<int64_t> ownerDims,
                                   ArrayRef<int64_t> physicalBlockShape,
                                   ArrayRef<int64_t> haloShape,
                                   ArrayRef<int64_t> logicalWorkerSlice) {
  if (!physicalLayoutMatchesRealizedLoopSteps(op, ownerDims, physicalBlockShape,
                                              logicalWorkerSlice))
    return false;
  applyPhysicalPlan(op, ownerDims, physicalBlockShape, haloShape,
                    logicalWorkerSlice);
  return true;
}

std::optional<SmallVector<int64_t, 4>>
orderPhysicalOwnerDimsByLoop(const sde::SuOutputLayoutFacts &outputPlan,
                             ArrayRef<int64_t> layoutOwnerDims,
                             unsigned loopRank) {
  if (layoutOwnerDims.empty() || outputPlan.loopDimToPhysicalDim.empty())
    return std::nullopt;

  SmallVector<int64_t, 4> orderedOwnerDims;
  orderedOwnerDims.reserve(layoutOwnerDims.size());
  for (unsigned loopDim = 0; loopDim < loopRank; ++loopDim) {
    if (loopDim >= outputPlan.loopDimToPhysicalDim.size())
      return std::nullopt;
    int64_t physicalDim = outputPlan.loopDimToPhysicalDim[loopDim];
    if (physicalDim < 0)
      continue;
    if (static_cast<size_t>(physicalDim) >= outputPlan.shape.size())
      return std::nullopt;
    if (llvm::is_contained(layoutOwnerDims, physicalDim))
      orderedOwnerDims.push_back(physicalDim);
  }
  if (orderedOwnerDims.size() != layoutOwnerDims.size())
    return std::nullopt;
  return orderedOwnerDims;
}

bool allExternalStoresCoverOwnerDims(sde::SdeSuIterateOp op,
                                     ArrayRef<int64_t> ownerDims,
                                     ArrayRef<int64_t> physicalDimToLoopDim) {
  if (!op || ownerDims.empty() || op.getBody().empty())
    return true;

  auto loopIvs = op.getLoopInductionVars();
  if (!loopIvs || loopIvs->empty())
    return false;

  SmallVector<int64_t, 4> physicalDimToSuLoopDim;
  if (std::optional<SmallVector<int64_t, 4>> derived =
          derivePhysicalDimToSuLoopDimFromExternalStores(op, ownerDims)) {
    physicalDimToSuLoopDim.assign(derived->begin(), derived->end());
  } else {
    physicalDimToSuLoopDim.assign(physicalDimToLoopDim.begin(),
                                  physicalDimToLoopDim.end());
  }

  bool sawExternalStore = false;
  bool rejected = false;
  op.getBody().walk([&](memref::StoreOp storeOp) {
    if (rejected)
      return;
    Value root =
        ::mlir::carts::ValueAnalysis::stripMemrefViewOps(storeOp.getMemref());
    if (!root || sde::isDefinedInside(op.getOperation(), root))
      return;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType) {
      rejected = true;
      return;
    }
    if (memrefType.getRank() == 0)
      return;

    sawExternalStore = true;
    OperandRange indices = storeOp.getIndices();
    for (auto [ownerSlot, ownerDim] : llvm::enumerate(ownerDims)) {
      if (ownerDim < 0 || static_cast<unsigned>(ownerDim) >= indices.size()) {
        rejected = true;
        return;
      }
      (void)ownerSlot;
      if (static_cast<size_t>(ownerDim) >= physicalDimToSuLoopDim.size()) {
        rejected = true;
        return;
      }
      int64_t loopDim = physicalDimToSuLoopDim[ownerDim];
      if (loopDim < 0 || static_cast<size_t>(loopDim) >= loopIvs->size()) {
        rejected = true;
        return;
      }
      if (!sde::isOwnerDependentIndex(indices[ownerDim], (*loopIvs)[loopDim])) {
        rejected = true;
        return;
      }
    }
  });

  return sawExternalStore && !rejected;
}

} // namespace mlir::carts::sde::distribution
