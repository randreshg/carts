///==========================================================================///
/// File: SuLoopAccessQuery.cpp
///
/// On-demand query helpers that replace the deleted loop-pattern-facts pass
/// attribute stamps. Each fact is recomputed from IR via SuLoopAccessAnalysis.
///==========================================================================///

#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "carts/utils/ArrayAttrUtils.h"
#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"

using namespace mlir;
using namespace mlir::carts;
using namespace mlir::carts::sde;

namespace {

static Value accessRoot(Value value) {
  if (!value)
    return {};
  if (isa<BaseMemRefType>(value.getType()))
    return ValueAnalysis::stripMemrefViewOps(value);
  return value;
}

static bool sameAccessRoot(Value lhs, Value rhs) {
  Value lhsRoot = accessRoot(lhs);
  Value rhsRoot = accessRoot(rhs);
  return lhsRoot && rhsRoot && lhsRoot == rhsRoot;
}

static bool hasSelfRead(const SuLoopAccessSummary &summary) {
  for (const MemrefAccessEntry &write : summary.writes)
    for (const MemrefAccessEntry &read : summary.reads)
      if (sameAccessRoot(write.memref, read.memref))
        return true;
  return false;
}

static bool attrMatchesValues(ArrayAttr attr, ArrayRef<int64_t> values) {
  std::optional<SmallVector<int64_t, 4>> parsed = readI64ArrayAttr(attr);
  return parsed && llvm::equal(*parsed, values);
}

static bool hasExplicitStencilFacts(SdeSuIterateOp op) {
  auto classification = op.getStructuredClassification();
  return classification &&
         *classification == SdeStructuredClassification::stencil &&
         op.getAccessMinOffsetsAttr() && op.getAccessMaxOffsetsAttr() &&
         op.getOwnerDimsAttr() && op.getWriteFootprintAttr();
}

static std::optional<SuNeighborhoodAccessInfo>
readExplicitStencilNeighborhood(SdeSuIterateOp op) {
  auto minOffsets = readI64ArrayAttr(op.getAccessMinOffsetsAttr());
  auto maxOffsets = readI64ArrayAttr(op.getAccessMaxOffsetsAttr());
  auto ownerDims = readI64ArrayAttr(op.getOwnerDimsAttr());
  auto writeFootprint = readI64ArrayAttr(op.getWriteFootprintAttr());
  if (!minOffsets || !maxOffsets || !ownerDims || !writeFootprint)
    return std::nullopt;

  SuNeighborhoodAccessInfo info;
  info.minOffsets.assign(minOffsets->begin(), minOffsets->end());
  info.maxOffsets.assign(maxOffsets->begin(), maxOffsets->end());
  info.ownerDims.assign(ownerDims->begin(), ownerDims->end());
  info.writeFootprint.assign(writeFootprint->begin(), writeFootprint->end());
  if (auto spatialDims = readI64ArrayAttr(op.getSpatialDimsAttr()))
    info.spatialDims.assign(spatialDims->begin(), spatialDims->end());
  else
    info.spatialDims = info.ownerDims;
  return info;
}

static unsigned countHaloDims(const SuNeighborhoodAccessInfo &info) {
  unsigned count = 0;
  for (auto [minOffset, maxOffset] : llvm::zip(info.minOffsets, info.maxOffsets))
    if (minOffset != 0 || maxOffset != 0)
      ++count;
  return count;
}

static bool hasHigherOrderHalo(const SuNeighborhoodAccessInfo &info) {
  for (auto [minOffset, maxOffset] : llvm::zip(info.minOffsets, info.maxOffsets))
    if (minOffset < -1 || maxOffset > 1)
      return true;
  return false;
}

static bool isWavefront2D(const SuLoopAccessSummary &summary,
                          const SuNeighborhoodAccessInfo &info) {
  if (info.ownerDims.size() != 2 || !hasSelfRead(summary))
    return false;

  SmallVector<bool, 4> sawNegative(summary.nest.ivs.size(), false);
  bool sawPositiveSelfReadOffset = false;
  for (const MemrefAccessEntry &write : summary.writes) {
    for (const MemrefAccessEntry &read : summary.reads) {
      if (!sameAccessRoot(write.memref, read.memref))
        continue;
      for (AffineExpr result : read.indexingMap.getResults()) {
        auto dimOffset = extractDimOffset(result);
        if (!dimOffset || !dimOffset->dim)
          continue;
        unsigned dim = *dimOffset->dim;
        if (dim >= sawNegative.size())
          continue;
        if (dimOffset->offset < 0)
          sawNegative[dim] = true;
        if (dimOffset->offset > 0)
          sawPositiveSelfReadOffset = true;
      }
    }
  }

  if (sawPositiveSelfReadOffset)
    return false;

  unsigned negativeOwnerDims = 0;
  for (int64_t dim : info.ownerDims)
    if (dim >= 0 && static_cast<size_t>(dim) < sawNegative.size() &&
        sawNegative[dim])
      ++negativeOwnerDims;
  return negativeOwnerDims == 2;
}

static bool hasParallelLeafCu(SdeSuIterateOp op) {
  if (!op || op.getBody().empty())
    return false;
  for (Operation &child : op.getBody().front().without_terminator()) {
    auto cuRegion = dyn_cast<SdeCuRegionOp>(child);
    if (cuRegion && cuRegion.getKind() == SdeCuKind::parallel)
      return true;
  }
  return false;
}

static bool isSiblingDistributedIntermediate(SdeSuIterateOp consumer,
                                             Value root) {
  if (!root)
    return false;
  Operation *scope = consumer->getParentOfType<ModuleOp>();
  if (!scope)
    return false;
  bool found = false;
  scope->walk([&](SdeSuIterateOp producer) {
    if (found || producer == consumer)
      return;
    bool writesRoot = false;
    producer.getBody().walk([&](memref::StoreOp storeOp) {
      if (writesRoot)
        return;
      if (ValueAnalysis::stripMemrefViewOps(storeOp.getMemref()) == root)
        writesRoot = true;
    });
    if (writesRoot)
      found = true;
  });
  return found;
}

static std::optional<SuPartialReductionFacts>
computePartialReductionFacts(SdeSuIterateOp op,
                             const SuLoopAccessSummary &summary,
                             SdeStructuredClassification classification) {
  SuPartialReductionFacts facts;

  if (classification == SdeStructuredClassification::elementwise_pipeline &&
      isOwnerLocalPipelineReduction(op)) {
    for (auto [dim, iteratorType] : llvm::enumerate(summary.iterTypes)) {
      if (iteratorType == utils::IteratorType::reduction)
        facts.reductionDims.push_back(static_cast<int64_t>(dim));
    }
    if (facts.reductionDims.empty())
      return facts;

    std::optional<SuOutputLayoutFacts> outputLayout =
        findCompatibleSuOutputLayoutFacts(summary);
    if (!outputLayout)
      return facts;

    for (auto [physicalDim, loopDim] :
         llvm::enumerate(outputLayout->physicalDimToLoopDim)) {
      if (loopDim < 0 ||
          static_cast<size_t>(loopDim) >= summary.iterTypes.size())
        continue;
      if (summary.iterTypes[loopDim] == utils::IteratorType::parallel)
        facts.ownerDims.push_back(static_cast<int64_t>(physicalDim));
    }
    if (!facts.ownerDims.empty())
      facts.hasPartialReduction = true;
    return facts;
  }

  if (classification != SdeStructuredClassification::matmul)
    return facts;

  std::optional<ContractionTilingCandidate> candidate =
      findContractionTilingCandidate(op);
  if (!candidate || !candidate->contractionExtent ||
      *candidate->contractionExtent <= 0)
    return facts;
  if (!isSiblingDistributedIntermediate(op, candidate->contractionInputRoot))
    return facts;

  facts.hasPartialReduction = true;
  facts.reductionDims.push_back(
      static_cast<int64_t>(candidate->reductionLoopDim));
  facts.ownerDims.assign(candidate->parallelLoopDims.begin(),
                         candidate->parallelLoopDims.end());
  return facts;
}

} // namespace

namespace mlir::carts::sde {

bool hasOnlyPointInPlaceSelfReads(const SuLoopAccessSummary &summary) {
  bool sawSelfRead = false;
  for (const MemrefAccessEntry &read : summary.reads) {
    bool readsWrittenRoot = false;
    bool matchesWriteMap = false;
    for (const MemrefAccessEntry &write : summary.writes) {
      if (!sameAccessRoot(write.memref, read.memref))
        continue;
      readsWrittenRoot = true;
      if (read.indexingMap != write.indexingMap)
        continue;
      auto loadOp = dyn_cast_or_null<memref::LoadOp>(read.op);
      auto storeOp = dyn_cast_or_null<memref::StoreOp>(write.op);
      if (loadOp && storeOp &&
          ValueAnalysis::sameDirectMemrefAccess(
              loadOp.getMemref(), loadOp.getIndices(), storeOp.getMemref(),
              storeOp.getIndices()))
        matchesWriteMap = true;
    }
    if (!readsWrittenRoot)
      continue;
    sawSelfRead = true;
    if (!matchesWriteMap)
      return false;
  }
  return sawSelfRead;
}

SdeStructuredClassification
resolveStructuredClassification(SdeSuIterateOp op,
                                const SuLoopAccessSummary &summary) {
  SdeStructuredClassification classification = summary.classification;
  if (hasExplicitStencilFacts(op))
    return SdeStructuredClassification::stencil;

  if (auto existingClassification = op.getStructuredClassification();
      existingClassification &&
      *existingClassification ==
          SdeStructuredClassification::elementwise_pipeline &&
      classification == SdeStructuredClassification::elementwise)
    return SdeStructuredClassification::elementwise_pipeline;

  if (auto existingClassification = op.getStructuredClassification();
      existingClassification &&
      *existingClassification == SdeStructuredClassification::reduction &&
      classification == SdeStructuredClassification::elementwise &&
      op.getReductionAccumulators().empty())
    return SdeStructuredClassification::reduction;

  if (auto existingClassification = op.getStructuredClassification();
      existingClassification &&
      classification == SdeStructuredClassification::reduction &&
      *existingClassification != SdeStructuredClassification::reduction &&
      op.getReductionAccumulators().empty())
    return *existingClassification;

  return classification;
}

std::optional<SdeStructuredClassification>
queryStructuredClassification(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary) {
    if (auto stamped = op.getStructuredClassification())
      return *stamped;
    return std::nullopt;
  }
  return resolveStructuredClassification(op, *summary);
}

SdePattern deriveSuPattern(const SuLoopAccessSummary &summary,
                           SdeStructuredClassification classification,
                           const SuNeighborhoodAccessInfo *neighborhood) {
  switch (classification) {
  case SdeStructuredClassification::elementwise:
    return SdePattern::uniform;
  case SdeStructuredClassification::elementwise_pipeline:
    return SdePattern::elementwise_pipeline;
  case SdeStructuredClassification::matmul:
    return SdePattern::matmul;
  case SdeStructuredClassification::reduction:
    return SdePattern::reduction;
  case SdeStructuredClassification::stencil:
    break;
  }

  if (!neighborhood)
    return SdePattern::stencil_tiling_nd;

  if (isWavefront2D(summary, *neighborhood))
    return SdePattern::wavefront_2d;
  if (hasHigherOrderHalo(*neighborhood))
    return SdePattern::higher_order_stencil;
  if (countHaloDims(*neighborhood) >= 3)
    return SdePattern::cross_dim_stencil_3d;
  return SdePattern::stencil_tiling_nd;
}

std::optional<SdePattern> querySuPattern(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return std::nullopt;
  SdeStructuredClassification classification =
      resolveStructuredClassification(op, *summary);

  if (auto pattern = op.getPattern()) {
    if (hasExplicitStencilFacts(op))
      return *pattern;
    if (*pattern == SdePattern::alternating_buffer_stencil)
      return *pattern;
  }

  const SuNeighborhoodAccessInfo *neighborhoodPtr = nullptr;
  std::optional<SuNeighborhoodAccessInfo> neighborhood;
  if (classification == SdeStructuredClassification::stencil) {
    neighborhood = queryNeighborhoodAccessInfo(op);
    if (neighborhood)
      neighborhoodPtr = &*neighborhood;
  }
  return deriveSuPattern(*summary, classification, neighborhoodPtr);
}

std::optional<SuNeighborhoodAccessInfo>
queryNeighborhoodAccessInfo(SdeSuIterateOp op) {
  if (hasExplicitStencilFacts(op))
    return readExplicitStencilNeighborhood(op);

  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return std::nullopt;
  if (resolveStructuredClassification(op, *summary) !=
      SdeStructuredClassification::stencil)
    return std::nullopt;
  return extractNeighborhoodAccessInfo(*summary);
}

bool queryInPlaceSafe(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return false;
  if (resolveStructuredClassification(op, *summary) !=
      SdeStructuredClassification::stencil)
    return false;

  StructuredMemoryEffectSummary effects =
      collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || !hasInPlaceSelfRead(effects))
    return false;
  if (!hasOnlyPointInPlaceSelfReads(*summary))
    return false;
  return hasParallelLeafCu(op);
}

bool queryInPlaceSharedState(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return false;

  StructuredMemoryEffectSummary effects =
      collectStructuredMemoryEffects(op.getBody());
  if (effects.hasUnknownEffects || !hasInPlaceSelfRead(effects))
    return false;
  if (hasOnlyPointInPlaceSelfReads(*summary))
    return false;

  auto classification = resolveStructuredClassification(op, *summary);
  if (classification == SdeStructuredClassification::stencil)
    return hasParallelLeafCu(op);
  return hasParallelLeafCu(op);
}

std::optional<SuPartialReductionFacts>
queryPartialReductionFacts(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return std::nullopt;
  SdeStructuredClassification classification =
      resolveStructuredClassification(op, *summary);
  return computePartialReductionFacts(op, *summary, classification);
}

} // namespace mlir::carts::sde
