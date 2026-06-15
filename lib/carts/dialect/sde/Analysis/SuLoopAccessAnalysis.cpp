///==========================================================================///
/// File: SuLoopAccessAnalysis.cpp
///
/// Reusable structural analysis for SDE scheduling-unit loops.
///
/// Umbrella translation unit: it owns the public query API
/// (analyzeSuLoopAccesses, findContractionTilingCandidate,
/// findCompatibleSuOutputLayoutFacts, hasRealizableOwnerStrip,
/// extractNeighborhoodAccessInfo, isOwnerLocalPipelineReduction,
/// hasDistinctExternalMatmulInputRoots, buildModuleSuAccessRelations) plus the
/// shape / owner-slice / write-support helpers those wrappers share. The
/// per-phase mechanics are carved into PerfectNestCollect.cpp,
/// MemrefAccessCollect.cpp, StructuredClassify.cpp, and
/// NeighborhoodAnalysis.cpp (declared in SuLoopAccessAnalysisDetail.h).
///==========================================================================///

#include "carts/dialect/sde/Analysis/SuLoopAccessAnalysis.h"
#include "SuLoopAccessAnalysisDetail.h"
#include "carts/dialect/sde/Analysis/AffineIndexUtils.h"
#include "carts/dialect/sde/Analysis/SdeAnalysisUtils.h"
#include "carts/dialect/sde/Utils/SdeCommittedFactUtils.h"
#include "carts/utils/ArrayAttrUtils.h"

#include "carts/utils/ValueAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineMemoryOpInterfaces.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/Utils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"

using namespace mlir;
using namespace mlir::carts;

namespace mlir::carts::sde {
namespace {

static void appendNestedForIvs(Block &body, SmallVectorImpl<Value> &ivs) {
  llvm::SmallPtrSet<Value, 8> seen;
  for (Value iv : ivs)
    seen.insert(iv);

  body.walk([&](scf::ForOp loop) {
    Value iv = loop.getInductionVar();
    if (seen.insert(iv).second)
      ivs.push_back(iv);
  });
  body.walk([&](affine::AffineForOp loop) {
    Value iv = loop.getInductionVar();
    if (seen.insert(iv).second)
      ivs.push_back(iv);
  });
}

static Value normalizeOutputRoot(Value value) {
  if (!value)
    return {};
  if (isa<BaseMemRefType>(value.getType()))
    return ::mlir::carts::ValueAnalysis::stripMemrefViewOps(value);
  return value;
}

static std::optional<SmallVector<int64_t, 4>> getStaticShape(Value value) {
  Value root = normalizeOutputRoot(value);
  if (!root)
    return std::nullopt;
  auto shapedType = dyn_cast<ShapedType>(root.getType());
  if (!shapedType || !shapedType.hasRank() || !shapedType.hasStaticShape())
    return std::nullopt;

  SmallVector<int64_t, 4> shape;
  shape.reserve(shapedType.getRank());
  for (int64_t dim : shapedType.getShape())
    shape.push_back(dim);
  return shape;
}

static bool hasAnyExactOwnerSliceAccess(SdeSuIterateOp iterOp, Value root,
                                        ArrayRef<int64_t> ownerDims) {
  if (!iterOp || !root || ownerDims.empty())
    return false;

  SmallVector<Value, 4> ownerIndexValues = collectOwnerIndexValues(iterOp);
  if (ownerIndexValues.empty())
    return false;

  bool sawExactOwnerAccess = false;
  auto checkIndices = [&](Value memref, OperandRange indices) {
    Value base = normalizeOutputRoot(memref);
    if (base != root || indices.empty())
      return;

    for (int64_t rawDim : ownerDims) {
      if (rawDim < 0 || static_cast<size_t>(rawDim) >= indices.size())
        return;
      if (!isExactOwnerIndex(indices[rawDim], ownerIndexValues))
        return;
    }
    sawExactOwnerAccess = true;
  };

  iterOp.getBody().walk([&](Operation *nested) {
    if (sawExactOwnerAccess)
      return WalkResult::interrupt();
    if (auto loadOp = dyn_cast<memref::LoadOp>(nested)) {
      if (!isa<MemRefType>(loadOp.getResult().getType()))
        checkIndices(loadOp.getMemref(), loadOp.getIndices());
      return sawExactOwnerAccess ? WalkResult::interrupt()
                                 : WalkResult::advance();
    }
    if (auto storeOp = dyn_cast<memref::StoreOp>(nested)) {
      if (!isa<MemRefType>(storeOp.getValueToStore().getType()))
        checkIndices(storeOp.getMemref(), storeOp.getIndices());
      return sawExactOwnerAccess ? WalkResult::interrupt()
                                 : WalkResult::advance();
    }
    return WalkResult::advance();
  });

  return sawExactOwnerAccess;
}

static std::optional<
    std::pair<SmallVector<int64_t, 4>, SmallVector<int64_t, 4>>>
buildLoopPhysicalDimMaps(AffineMap map, unsigned numLoops, unsigned rank) {
  if (map.getNumResults() != rank)
    return std::nullopt;

  SmallVector<int64_t, 4> loopToPhysical(numLoops, -1);
  SmallVector<int64_t, 4> physicalToLoop(rank, -1);
  bool mappedAnyDim = false;

  for (unsigned physicalDim = 0; physicalDim < rank; ++physicalDim) {
    std::optional<AffineDimOffset> dimOffset =
        extractDimOffset(map.getResult(physicalDim));
    if (!dimOffset)
      return std::nullopt;
    if (!dimOffset->dim)
      continue;
    if (dimOffset->offset != 0)
      return std::nullopt;

    unsigned loopDim = *dimOffset->dim;
    if (loopDim >= numLoops)
      return std::nullopt;
    if (loopToPhysical[loopDim] >= 0 || physicalToLoop[physicalDim] >= 0)
      return std::nullopt;
    loopToPhysical[loopDim] = static_cast<int64_t>(physicalDim);
    physicalToLoop[physicalDim] = static_cast<int64_t>(loopDim);
    mappedAnyDim = true;
  }

  if (!mappedAnyDim)
    return std::nullopt;
  return std::make_pair(std::move(loopToPhysical), std::move(physicalToLoop));
}

static unsigned countNonDegenerateDims(ArrayRef<int64_t> shape) {
  unsigned count = 0;
  for (int64_t extent : shape)
    if (extent > 1)
      ++count;
  return count;
}

struct WriteSupport {
  unsigned coverageRank = 0;
  bool fullRank = false;
};

static WriteSupport
computeFullRootWriteSupport(AffineMap map,
                            ArrayRef<utils::IteratorType> iterTypes,
                            ArrayRef<int64_t> staticShape) {
  if (map.getNumResults() != staticShape.size())
    return {};

  unsigned nonDegenerateRank = countNonDegenerateDims(staticShape);
  llvm::SmallBitVector coveredPhysical(staticShape.size(), false);
  llvm::SmallBitVector usedLoopDims(iterTypes.size(), false);

  for (unsigned physicalDim = 0; physicalDim < staticShape.size();
       ++physicalDim) {
    if (staticShape[physicalDim] <= 1)
      continue;

    std::optional<AffineDimOffset> dimOffset =
        extractDimOffset(map.getResult(physicalDim));
    if (!dimOffset || !dimOffset->dim || dimOffset->offset != 0)
      continue;

    unsigned loopDim = *dimOffset->dim;
    if (loopDim >= iterTypes.size() ||
        iterTypes[loopDim] != utils::IteratorType::parallel)
      continue;

    if (usedLoopDims.test(loopDim))
      return {static_cast<unsigned>(coveredPhysical.count()), false};
    usedLoopDims.set(loopDim);
    coveredPhysical.set(physicalDim);
  }

  unsigned coverageRank = coveredPhysical.count();
  return {coverageRank, coverageRank == nonDegenerateRank};
}

} // namespace

bool isOwnerLocalPipelineReduction(SdeSuIterateOp iterOp) {
  if (iterOp.getReductionAccumulators().size() != 0)
    return false;
  if (iterOp.getLowerBounds().size() != 1)
    return false;

  std::optional<LoopIndexedOutputShape> outputShape =
      findLoopIndexedOutputShape(iterOp);
  if (!outputShape || outputShape->ownerPhysicalDims.empty())
    return false;

  StructuredMemoryEffectSummary effects =
      collectStructuredMemoryEffects(iterOp.getBody());
  if (effects.hasUnknownEffects || effects.writes.empty() ||
      !effects.writes.contains(outputShape->root))
    return false;

  // Owner-local pipeline reductions may be realized with compute-block
  // dependency views. Every external read therefore has to be provably inside
  // the same owner slice, not merely dependent on the owner IV. Triangular
  // self-Gram style kernels read both data[i, *] and data[j, *] for j > i;
  // slicing those reads to the i owner block is out of bounds on multinode
  // runs.
  for (Value read : effects.reads) {
    if (isDefinedInside(iterOp.getOperation(), read))
      continue;
    if (!hasAnyExactOwnerSliceAccess(iterOp, read,
                                     outputShape->ownerPhysicalDims))
      continue;
    if (!allRootAccessesStayWithinOwnerSlice(iterOp, read,
                                             outputShape->ownerPhysicalDims))
      return false;
  }

  for (Value written : effects.writes) {
    if (isDefinedInside(iterOp.getOperation(), written))
      continue;
    if (!effects.reads.contains(written))
      continue;
    if (!allRootAccessesStayWithinOwnerSlice(iterOp, written,
                                             outputShape->ownerPhysicalDims))
      return false;
  }

  return true;
}

bool hasDistinctExternalMatmulInputRoots(SdeSuIterateOp iterOp) {
  auto hasDistinctExternalReadOnlyRoots = [&]() {
    StructuredMemoryEffectSummary effects =
        collectStructuredMemoryEffects(iterOp.getBody());
    if (effects.hasUnknownEffects)
      return false;

    llvm::DenseSet<Value> inputRoots;
    iterOp.getBody().walk([&](memref::LoadOp loadOp) {
      if (isa<MemRefType>(loadOp.getResult().getType()))
        return;
      Value root = normalizeOutputRoot(loadOp.getMemref());
      if (!root || isDefinedInside(iterOp.getOperation(), root) ||
          effects.writes.contains(root))
        return;
      inputRoots.insert(root);
    });
    return inputRoots.size() >= 2;
  };

  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(iterOp);
  if (!summary)
    return hasDistinctExternalReadOnlyRoots();

  SmallVector<unsigned, 2> parallelDims;
  SmallVector<unsigned, 1> reductionDims;
  for (auto [dim, type] : llvm::enumerate(summary->iterTypes)) {
    if (type == utils::IteratorType::parallel)
      parallelDims.push_back(dim);
    else
      reductionDims.push_back(dim);
  }
  if (parallelDims.size() != 2 || reductionDims.size() != 1 ||
      summary->nest.ivs.size() != 3)
    return hasDistinctExternalReadOnlyRoots();

  unsigned numDims = summary->nest.ivs.size();
  llvm::SmallBitVector lhsDims(numDims);
  lhsDims.set(parallelDims[0]);
  lhsDims.set(reductionDims[0]);
  llvm::SmallBitVector rhsDims(numDims);
  rhsDims.set(reductionDims[0]);
  rhsDims.set(parallelDims[1]);

  Value lhsRoot;
  Value rhsRoot;
  llvm::DenseSet<Value> externalWriteRoots;
  for (const MemrefAccessEntry &write : summary->writes) {
    Value root = normalizeOutputRoot(write.memref);
    if (root && !isDefinedInside(iterOp.getOperation(), root))
      externalWriteRoots.insert(root);
  }

  for (const MemrefAccessEntry &read : summary->reads) {
    Value root = normalizeOutputRoot(read.memref);
    if (!root || isDefinedInside(iterOp.getOperation(), root) ||
        externalWriteRoots.contains(root))
      continue;

    llvm::SmallBitVector used = detail::getUsedDims(read.indexingMap, numDims);
    if (used == lhsDims) {
      if (!lhsRoot)
        lhsRoot = root;
      else if (lhsRoot != root)
        return false;
    }
    if (used == rhsDims) {
      if (!rhsRoot)
        rhsRoot = root;
      else if (rhsRoot != root)
        return false;
    }
  }

  if (lhsRoot && rhsRoot && lhsRoot != rhsRoot)
    return true;
  return hasDistinctExternalReadOnlyRoots();
}

std::optional<ContractionTilingCandidate>
findContractionTilingCandidate(SdeSuIterateOp iterOp) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(iterOp);
  if (!summary)
    return std::nullopt;
  if (summary->classification != SdeStructuredClassification::matmul)
    return std::nullopt;

  SmallVector<unsigned, 2> parallelDims;
  SmallVector<unsigned, 1> reductionDims;
  for (auto [dim, type] : llvm::enumerate(summary->iterTypes)) {
    if (type == utils::IteratorType::parallel)
      parallelDims.push_back(dim);
    else
      reductionDims.push_back(dim);
  }
  if (parallelDims.size() != 2 || reductionDims.size() != 1 ||
      summary->nest.ivs.size() != 3)
    return std::nullopt;

  unsigned numDims = summary->nest.ivs.size();
  llvm::SmallBitVector lhsDims(numDims);
  lhsDims.set(parallelDims[0]);
  lhsDims.set(reductionDims[0]);
  llvm::SmallBitVector rhsDims(numDims);
  rhsDims.set(reductionDims[0]);
  rhsDims.set(parallelDims[1]);

  llvm::DenseSet<Value> externalWriteRoots;
  for (const MemrefAccessEntry &write : summary->writes) {
    Value root = normalizeOutputRoot(write.memref);
    if (root && !isDefinedInside(iterOp.getOperation(), root))
      externalWriteRoots.insert(root);
  }

  Value lhsRoot;
  Value rhsRoot;
  for (const MemrefAccessEntry &read : summary->reads) {
    Value root = normalizeOutputRoot(read.memref);
    if (!root || isDefinedInside(iterOp.getOperation(), root) ||
        externalWriteRoots.contains(root))
      continue;

    llvm::SmallBitVector used = detail::getUsedDims(read.indexingMap, numDims);
    if (used == lhsDims) {
      if (!lhsRoot)
        lhsRoot = root;
      else if (lhsRoot != root)
        return std::nullopt;
    }
    if (used == rhsDims) {
      if (!rhsRoot)
        rhsRoot = root;
      else if (rhsRoot != root)
        return std::nullopt;
    }
  }

  // The contraction-dim input must be a single distinct external root that is
  // not also the lhs (rules out self-Gram shapes).
  if (!lhsRoot || !rhsRoot || lhsRoot == rhsRoot)
    return std::nullopt;

  ContractionTilingCandidate candidate;
  candidate.contractionInputRoot = rhsRoot;
  candidate.reductionLoopDim = reductionDims[0];
  candidate.parallelLoopDims.assign(parallelDims.begin(), parallelDims.end());

  // Recover the physical contraction position and its static extent from the
  // contraction-dim input root. The result position carrying the reduction dim
  // indexes the contracted axis; do not assume a canonical operand order.
  if (auto rhsShape = getStaticShape(rhsRoot)) {
    for (const MemrefAccessEntry &read : summary->reads) {
      if (normalizeOutputRoot(read.memref) != rhsRoot)
        continue;
      if (detail::getUsedDims(read.indexingMap, numDims) != rhsDims)
        continue;
      for (auto [pos, result] :
           llvm::enumerate(read.indexingMap.getResults())) {
        auto dimOffset = extractDimOffset(result);
        if (dimOffset && dimOffset->dim &&
            *dimOffset->dim == reductionDims[0] && pos < rhsShape->size()) {
          candidate.contractionInputPhysicalDim = static_cast<unsigned>(pos);
          candidate.contractionExtent = (*rhsShape)[pos];
          break;
        }
      }
      break;
    }
  }

  return candidate;
}

std::optional<SuLoopAccessSummary>
analyzeSuLoopAccesses(SdeSuIterateOp iterOp) {
  MLIRContext *ctx = iterOp.getContext();
  SuLoopAccessSummary summary;
  if (!detail::collectPerfectNest(iterOp, summary.nest))
    return std::nullopt;
  if (!summary.nest.innermostBody || summary.nest.ivs.empty())
    return std::nullopt;
  appendNestedForIvs(*summary.nest.innermostBody, summary.nest.ivs);

  if (!detail::collectMemrefAccesses(
          iterOp.getOperation(), *summary.nest.innermostBody, summary.nest.ivs,
          summary.reads, summary.writes, ctx))
    return std::nullopt;

  summary.outputMaps.reserve(summary.writes.size() +
                             iterOp.getReductionAccumulators().size());
  for (const MemrefAccessEntry &write : summary.writes)
    summary.outputMaps.push_back(write.indexingMap);
  for (Value ignored : iterOp.getReductionAccumulators()) {
    (void)ignored;
    summary.outputMaps.push_back(
        AffineMap::get(summary.nest.ivs.size(), 0, {}, ctx));
  }

  if (summary.outputMaps.empty())
    return std::nullopt;

  // If no read or write indexing map references any loop IV (every result is a
  // constant), the analyzer cannot prove what this loop is doing — typically
  // the only loads/stores we could see were leaf accesses behind opaque
  // pointer-of-pointer indirection (e.g. `float ****` function arguments).
  // Bail rather than fall through to `classifyPattern`, which would otherwise
  // mark every IV as a reduction dim and hard-block Tiling (see
  // Tiling.cpp::isTilingCandidate, `case reduction: return false`).
  auto isConstantMap = [](AffineMap map) {
    return llvm::all_of(map.getResults(), [](AffineExpr e) {
      return isa<AffineConstantExpr>(e);
    });
  };
  bool readsAllConstant =
      llvm::all_of(summary.reads, [&](const MemrefAccessEntry &e) {
        return isConstantMap(e.indexingMap);
      });
  bool writesAllConstant =
      llvm::all_of(summary.writes, [&](const MemrefAccessEntry &e) {
        return isConstantMap(e.indexingMap);
      });
  if (readsAllConstant && writesAllConstant)
    return std::nullopt;

  detail::computeIteratorTypes(summary.nest.ivs.size(), summary.outputMaps,
                               summary.iterTypes);
  summary.classification =
      detail::classifyPattern(summary.reads, summary.outputMaps,
                              summary.iterTypes, summary.nest.ivs.size());
  if (summary.classification == SdeStructuredClassification::reduction &&
      isOwnerLocalPipelineReduction(iterOp))
    summary.classification = SdeStructuredClassification::elementwise_pipeline;
  summary.supportsReductionCarrier = detail::supportsReductionCarrierSubset(
      iterOp, summary.nest, summary.reads, summary.writes);
  return summary;
}

std::optional<SuNeighborhoodAccessInfo>
extractNeighborhoodAccessInfo(const SuLoopAccessSummary &summary) {
  return detail::extractNeighborhoodAccessInfo(summary.reads,
                                               summary.nest.ivs.size());
}

std::optional<SuOutputLayoutFacts>
findCompatibleSuOutputLayoutFacts(const SuLoopAccessSummary &summary) {
  if (!summary.nest.rootIterOp || summary.writes.empty() ||
      summary.nest.ivs.empty())
    return std::nullopt;

  std::optional<SuOutputLayoutFacts> selected;
  Operation *rootOp = summary.nest.rootIterOp;

  for (const MemrefAccessEntry &write : summary.writes) {
    Value root = normalizeOutputRoot(write.memref);
    if (!root || isDefinedInside(rootOp, root))
      continue;
    auto memrefType = dyn_cast<MemRefType>(root.getType());
    if (!memrefType || memrefType.getRank() == 0)
      continue;

    std::optional<SmallVector<int64_t, 4>> shape = getStaticShape(root);
    if (!shape || shape->empty())
      return std::nullopt;

    WriteSupport support = computeFullRootWriteSupport(
        write.indexingMap, summary.iterTypes, *shape);
    if (!support.fullRank)
      return std::nullopt;

    auto maps = buildLoopPhysicalDimMaps(
        write.indexingMap, summary.nest.ivs.size(), shape->size());
    if (!maps)
      return std::nullopt;

    SuOutputLayoutFacts candidate;
    candidate.root = root;
    candidate.shape = std::move(*shape);
    candidate.loopDimToPhysicalDim = std::move(maps->first);
    candidate.physicalDimToLoopDim = std::move(maps->second);

    if (!selected) {
      selected = std::move(candidate);
      continue;
    }

    if (candidate.shape != selected->shape ||
        candidate.loopDimToPhysicalDim != selected->loopDimToPhysicalDim ||
        candidate.physicalDimToLoopDim != selected->physicalDimToLoopDim)
      return std::nullopt;
  }

  return selected;
}

std::optional<SuOutputLayoutFacts>
findCompatibleSuOutputLayoutFacts(SdeSuIterateOp op) {
  std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
  if (!summary)
    return std::nullopt;
  return findCompatibleSuOutputLayoutFacts(*summary);
}

bool hasRealizableOwnerStrip(SdeSuIterateOp op) {
  // Restricted to inPlaceSafe (proven point-local self-read) stencils so it
  // never reinterprets a Gauss-Seidel / inPlaceSharedState owner shape.
  if (!queryInPlaceSafe(op))
    return false;
  auto classification = queryStructuredClassification(op);
  if (!classification ||
      *classification != SdeStructuredClassification::stencil)
    return false;

  unsigned loopRank = op.getLowerBounds().size();
  if (loopRank == 0)
    return false;

  // (1) Realized form: an owner strip/tile physical layout has already been
  // committed whose owner dimensions fit within the realized loop rank. After
  // tiling the loop carries inner element loops, so the access-derived layout
  // recovery below no longer matches; the committed owner-dim count is the
  // authoritative realizability signal at that point.
  if (std::optional<sde::CommittedSuPhysicalLayout> committed =
          sde::recoverCommittedPhysicalLayout(op)) {
    if (!committed->ownerDims.empty() &&
        committed->ownerDims.size() <= loopRank &&
        llvm::all_of(committed->ownerDims, [](int64_t d) { return d >= 0; }))
      return true;
  }

  // (2) Pre-fact form: at least one parallel loop band maps 1:1 onto a static
  // output physical dimension; that band is the realizable owner strip. The
  // wider access footprint still has to be carried as read-only halo movement.
  std::optional<SuOutputLayoutFacts> layoutFacts =
      findCompatibleSuOutputLayoutFacts(op);
  if (!layoutFacts)
    return false;
  for (unsigned loopDim = 0;
       loopDim < loopRank && loopDim < layoutFacts->loopDimToPhysicalDim.size();
       ++loopDim) {
    int64_t physicalDim = layoutFacts->loopDimToPhysicalDim[loopDim];
    if (physicalDim >= 0 &&
        static_cast<size_t>(physicalDim) < layoutFacts->shape.size())
      return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// Module-scoped layout-assignment access model.
//===----------------------------------------------------------------------===//

namespace {

// Classify one physical access position from its affine result expression and
// the loop iterator types. Pattern-free: the kind comes only from whether the
// expression is a single loop dim (and which iterator type that dim has) plus
// any constant offset.
static ArrayPositionUse
classifyPositionUse(AffineExpr result, ArrayRef<utils::IteratorType> iterTypes,
                    unsigned suId, bool isWrite) {
  ArrayPositionUse use;
  use.suId = suId;
  use.isWrite = isWrite;

  std::optional<AffineDimOffset> dimOffset = extractDimOffset(result);
  if (!dimOffset || !dimOffset->dim) {
    use.kind = ArrayDimKind::broadcast;
    return use;
  }

  unsigned loopDim = *dimOffset->dim;
  if (loopDim >= iterTypes.size()) {
    use.kind = ArrayDimKind::broadcast;
    return use;
  }
  use.loopDim = loopDim;

  if (iterTypes[loopDim] == utils::IteratorType::reduction) {
    use.kind = ArrayDimKind::reductionIndexed;
    return use;
  }

  use.kind = dimOffset->offset != 0 ? ArrayDimKind::parallelHalo
                                    : ArrayDimKind::parallelIndexed;
  return use;
}

// Record one access entry's per-position uses into the array profile.
static void recordAccessEntry(ModuleSuAccessRelations &relations,
                              const MemrefAccessEntry &entry,
                              ArrayRef<utils::IteratorType> iterTypes,
                              unsigned suId, bool isWrite) {
  Value root = normalizeOutputRoot(entry.memref);
  if (!root)
    return;
  // External arrays only: scratch defined inside the scheduling unit is not a
  // distribution candidate.
  if (isDefinedInside(relations.schedulingUnits[suId].getOperation(), root))
    return;

  std::optional<SmallVector<int64_t, 4>> shape = getStaticShape(root);
  if (!shape || shape->empty())
    return;
  unsigned rank = shape->size();
  if (entry.indexingMap.getNumResults() != rank)
    return;

  ArrayAccessProfile &profile = relations.profiles[root];
  if (!profile.root) {
    profile.root = root;
    profile.rank = rank;
    profile.staticShape = *shape;
    profile.nonDegenerateRank = countNonDegenerateDims(*shape);
    profile.positionUses.assign(rank, {});
  }
  if (profile.rank != rank || profile.staticShape != *shape)
    return;

  WriteSupport writeSupport;
  if (isWrite)
    writeSupport =
        computeFullRootWriteSupport(entry.indexingMap, iterTypes, *shape);

  if (isWrite) {
    profile.hasWriter = true;
    bool hadFullRankWriter = profile.hasFullRankWriter;
    bool betterWriter =
        !profile.writerSuId ||
        writeSupport.coverageRank > profile.maxWriteCoverageRank ||
        (writeSupport.fullRank && !hadFullRankWriter);
    if (betterWriter)
      profile.writerSuId = suId;
    profile.maxWriteCoverageRank =
        std::max(profile.maxWriteCoverageRank, writeSupport.coverageRank);
    profile.hasFullRankWriter |= writeSupport.fullRank;
  } else {
    profile.hasReader = true;
  }

  for (unsigned pos = 0; pos < rank; ++pos) {
    ArrayPositionUse use = classifyPositionUse(entry.indexingMap.getResult(pos),
                                               iterTypes, suId, isWrite);
    if (use.loopDim &&
        *use.loopDim < relations.schedulingUnits[suId].getLowerBounds().size())
      use.isSchedulingLoopDim = true;
    if (isWrite) {
      use.writeCoverageRank = writeSupport.coverageRank;
      use.fullRankWrite = writeSupport.fullRank;
    }
    profile.positionUses[pos].push_back(use);
  }
}

} // namespace

ModuleSuAccessRelations buildModuleSuAccessRelations(Operation *moduleOp) {
  ModuleSuAccessRelations relations;
  if (!moduleOp)
    return relations;

  // Assign stable scheduling-unit ids in walk order so writer/reader joins are
  // deterministic across runs.
  moduleOp->walk(
      [&](SdeSuIterateOp op) { relations.schedulingUnits.push_back(op); });

  for (auto [suId, op] : llvm::enumerate(relations.schedulingUnits)) {
    std::optional<SuLoopAccessSummary> summary = analyzeSuLoopAccesses(op);
    if (!summary)
      continue;
    for (const MemrefAccessEntry &write : summary->writes)
      recordAccessEntry(relations, write, summary->iterTypes,
                        static_cast<unsigned>(suId), /*isWrite=*/true);
    for (const MemrefAccessEntry &read : summary->reads)
      recordAccessEntry(relations, read, summary->iterTypes,
                        static_cast<unsigned>(suId), /*isWrite=*/false);
  }

  return relations;
}

} // namespace mlir::carts::sde
