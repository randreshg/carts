///==========================================================================///
/// File: PatternDiscovery.cpp
///
/// Metadata-guided seed/refine pass for pre-DB pattern contracts.
///
/// The pass is intentionally conservative:
/// - metadata is treated as a hint, not as proof
/// - seed mode only stamps families when loop and memref metadata agree
/// - refine mode can strengthen generic stencil contracts when the metadata
///   clearly indicates a multi-dimensional stencil shape
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/analysis/AnalysisManager.h"
#include "arts/analysis/metadata/MetadataManager.h"
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/transforms/kernel/KernelTransform.h"
#define GEN_PASS_DEF_PATTERNDISCOVERY
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"
#include "mlir/Pass/Pass.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/CallInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <cstdlib>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(pattern_discovery);

using namespace mlir;
using namespace mlir::arts;

namespace {

enum class DiscoveryMode { Seed, Refine };

struct DiscoveryEvidence {
  const LoopMetadata *loopMeta = nullptr;
  SmallVector<const MemrefMetadata *> memrefs;
  bool loopAllowsParallelFamily = false;
  bool loopHasReductions = false;
  bool loopHasDeps = false;
  unsigned totalMemrefs = 0;
  unsigned affineMemrefs = 0;
  unsigned uniformMemrefs = 0;
  unsigned stencilMemrefs = 0;
  unsigned multiDimStencilMemrefs = 0;
  unsigned locallyVerifiedStencilMemrefs = 0;
  unsigned locallyVerifiedMultiDimStencilMemrefs = 0;
};

static int64_t revisionForMode(DiscoveryMode mode) {
  return mode == DiscoveryMode::Refine ? 2 : 1;
}

static bool isLoopParallelCandidate(const LoopMetadata &meta) {
  if (meta.hasReductions)
    return false;
  if (meta.parallelClassification &&
      *meta.parallelClassification ==
          LoopMetadata::ParallelClassification::Sequential)
    return false;
  if (meta.hasInterIterationDeps && *meta.hasInterIterationDeps)
    return false;
  return meta.potentiallyParallel;
}

static unsigned countSpatialLikeDims(const MemrefMetadata &meta) {
  unsigned count = 0;
  for (auto pattern : meta.dimAccessPatterns) {
    if (pattern == MemrefMetadata::DimAccessPatternType::UnitStride ||
        pattern == MemrefMetadata::DimAccessPatternType::Affine)
      ++count;
  }
  return count;
}

static bool isUniformMemref(const MemrefMetadata &meta) {
  if (meta.dominantAccessPattern &&
      *meta.dominantAccessPattern == AccessPatternType::Sequential)
    return true;
  if (meta.hasUniformAccess && *meta.hasUniformAccess)
    return true;
  if (meta.hasStrideOneAccess && *meta.hasStrideOneAccess)
    return true;
  return false;
}

static bool isStencilMemref(const MemrefMetadata &meta) {
  return meta.dominantAccessPattern &&
         *meta.dominantAccessPattern == AccessPatternType::Stencil;
}

static bool isMultiDimStencilMemref(const MemrefMetadata &meta) {
  if (!isStencilMemref(meta))
    return false;

  if (meta.rank && *meta.rank >= 2)
    return true;

  return countSpatialLikeDims(meta) >= 2;
}

static Operation *resolveAllocLikeSource(Value value) {
  Value stripped = ValueAnalysis::stripMemrefViewOps(value);
  Operation *def = stripped ? stripped.getDefiningOp() : nullptr;
  if (def && isa<memref::AllocOp, memref::AllocaOp>(def))
    return def;
  return nullptr;
}

static Value getAccessedMemref(Operation *op) {
  if (auto load = dyn_cast<memref::LoadOp>(op))
    return load.getMemref();
  if (auto store = dyn_cast<memref::StoreOp>(op))
    return store.getMemref();
  if (auto load = dyn_cast<affine::AffineLoadOp>(op))
    return load.getMemref();
  if (auto store = dyn_cast<affine::AffineStoreOp>(op))
    return store.getMemref();
  return Value();
}

static SmallVector<Value> getAccessIndices(Operation *op) {
  SmallVector<Value> indices;
  if (auto load = dyn_cast<memref::LoadOp>(op)) {
    indices.append(load.getIndices().begin(), load.getIndices().end());
  } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
    indices.append(store.getIndices().begin(), store.getIndices().end());
  } else if (auto load = dyn_cast<affine::AffineLoadOp>(op)) {
    indices.append(load.getMapOperands().begin(), load.getMapOperands().end());
  } else if (auto store = dyn_cast<affine::AffineStoreOp>(op)) {
    indices.append(store.getMapOperands().begin(),
                   store.getMapOperands().end());
  }
  return indices;
}

struct LocalStencilEvidence {
  bool isStencil = false;
  bool isMultiDimStencil = false;
  std::optional<StencilNDPatternContract> explicitContract;
};

static bool isPureOp(Operation *op) {
  if (!op)
    return false;
  if (isa<arts::ForOp, arts::YieldOp, memref::LoadOp, memref::StoreOp,
          affine::AffineLoadOp, affine::AffineStoreOp, affine::AffineApplyOp,
          scf::ForOp, scf::YieldOp, affine::AffineForOp, affine::AffineYieldOp>(
          op))
    return true;
  if (isa<CallOpInterface>(op))
    return false;
  if (auto iface = dyn_cast<MemoryEffectOpInterface>(op))
    return iface.hasNoEffect();
  return op->getNumRegions() == 0;
}

static bool collectSpatialNestIvs(arts::ForOp artsFor,
                                  SmallVector<Value, 4> &ivs,
                                  Block *&spatialBody) {
  if (!artsFor || artsFor.getBody()->getNumArguments() == 0)
    return false;
  ivs.push_back(artsFor.getBody()->getArgument(0));

  Block *block = artsFor.getBody();
  while (block) {
    SmallVector<Operation *, 2> nestedFors;
    for (Operation &op : block->without_terminator()) {
      if (isa<scf::ForOp, affine::AffineForOp>(&op)) {
        nestedFors.push_back(&op);
        continue;
      }
      if (!isPureOp(&op))
        return false;
    }

    if (nestedFors.size() != 1)
      break;

    Operation *nestedLoop = nestedFors.front();
    if (auto scfFor = dyn_cast<scf::ForOp>(nestedLoop)) {
      ivs.push_back(scfFor.getInductionVar());
      block = scfFor.getBody();
      continue;
    }
    auto affineFor = cast<affine::AffineForOp>(nestedLoop);
    ivs.push_back(affineFor.getInductionVar());
    block = affineFor.getBody();
  }

  spatialBody = block;
  return !ivs.empty() && spatialBody;
}

static const LoopMetadata *
resolveLoopMetadataForArtsFor(arts::ForOp forOp, MetadataManager &manager) {
  if (!forOp)
    return nullptr;

  if (const LoopMetadata *meta = manager.getLoopMetadata(forOp.getOperation()))
    return meta;

  const LoopMetadata *nestedMeta = nullptr;
  forOp.getBody()->walk([&](Operation *op) {
    if (nestedMeta || !isa<scf::ForOp, affine::AffineForOp>(op))
      return;
    nestedMeta = manager.getLoopMetadata(op);
  });
  return nestedMeta;
}

static LocalStencilEvidence collectLocalStencilEvidence(arts::ForOp forOp,
                                                        int64_t revision) {
  LocalStencilEvidence evidence;
  if (auto explicitContract = matchExplicitStencilNDContract(forOp, revision)) {
    evidence.isStencil = true;
    evidence.isMultiDimStencil = explicitContract->isMultiDimensional();
    evidence.explicitContract = std::move(explicitContract);
    return evidence;
  }
  Block *body = forOp.getBody();
  if (!body)
    return evidence;

  SmallVector<Value, 4> spatialIvs;
  Block *spatialBody = nullptr;
  if (!collectSpatialNestIvs(forOp, spatialIvs, spatialBody)) {
    spatialIvs.append(body->getArguments().begin(), body->getArguments().end());
  }

  DenseMap<Operation *, SmallVector<llvm::DenseSet<int64_t>, 4>> offsetsByAlloc;

  body->walk([&](Operation *op) {
    Value memref = getAccessedMemref(op);
    if (!memref)
      return;

    Operation *allocLike = resolveAllocLikeSource(memref);
    if (!allocLike)
      return;

    SmallVector<Value> indices = getAccessIndices(op);
    auto &dimOffsets = offsetsByAlloc[allocLike];
    if (dimOffsets.size() < indices.size())
      dimOffsets.resize(indices.size());

    for (auto [dim, index] : llvm::enumerate(indices)) {
      Value strippedIndex = ValueAnalysis::stripNumericCasts(index);
      for (Value iv : spatialIvs) {
        if (!ValueAnalysis::dependsOn(strippedIndex, iv) && strippedIndex != iv)
          continue;

        int64_t constantOffset = 0;
        Value base = ValueAnalysis::stripNumericCasts(
            ValueAnalysis::stripConstantOffset(strippedIndex, &constantOffset));
        if (base != iv)
          continue;

        dimOffsets[dim].insert(constantOffset);
        break;
      }
    }
  });

  for (const auto &[allocLike, dimOffsets] : offsetsByAlloc) {
    unsigned stencilDims = 0;
    for (const auto &offsets : dimOffsets) {
      if (offsets.size() < 2)
        continue;
      bool hasNonZeroOffset =
          llvm::any_of(offsets, [](int64_t offset) { return offset != 0; });
      if (!hasNonZeroOffset)
        continue;
      ++stencilDims;
    }

    if (stencilDims == 0)
      continue;

    evidence.isStencil = true;
    if (stencilDims > 1)
      evidence.isMultiDimStencil = true;
  }

  return evidence;
}

static DiscoveryEvidence
collectEvidence(arts::ForOp forOp, MetadataManager &manager,
                const LocalStencilEvidence &localStencil) {
  DiscoveryEvidence evidence;
  evidence.loopMeta = resolveLoopMetadataForArtsFor(forOp, manager);
  if (!evidence.loopMeta)
    return evidence;

  evidence.loopAllowsParallelFamily =
      isLoopParallelCandidate(*evidence.loopMeta);
  evidence.loopHasReductions = evidence.loopMeta->hasReductions;
  evidence.loopHasDeps = evidence.loopMeta->hasInterIterationDeps &&
                         *evidence.loopMeta->hasInterIterationDeps;

  llvm::SmallPtrSet<Operation *, 8> seenAllocs;
  forOp.getBody()->walk([&](Operation *op) {
    Value memref = getAccessedMemref(op);
    if (!memref)
      return;

    Operation *allocLike = resolveAllocLikeSource(memref);
    if (!allocLike || !seenAllocs.insert(allocLike).second)
      return;

    if (const MemrefMetadata *meta = manager.getMemrefMetadata(allocLike)) {
      evidence.memrefs.push_back(meta);
      ++evidence.totalMemrefs;

      if ((meta->allAccessesAffine && *meta->allAccessesAffine) ||
          (meta->hasAffineAccesses && *meta->hasAffineAccesses))
        ++evidence.affineMemrefs;
      if (isUniformMemref(*meta))
        ++evidence.uniformMemrefs;
      if (isStencilMemref(*meta) && localStencil.isStencil)
        ++evidence.stencilMemrefs;
      if (isMultiDimStencilMemref(*meta) && localStencil.isMultiDimStencil)
        ++evidence.multiDimStencilMemrefs;
      if (localStencil.isStencil)
        ++evidence.locallyVerifiedStencilMemrefs;
      if (localStencil.isMultiDimStencil)
        ++evidence.locallyVerifiedMultiDimStencilMemrefs;
    }
  });

  return evidence;
}

static std::optional<ArtsDepPattern>
chooseMetadataCandidate(const DiscoveryEvidence &evidence) {
  if (!evidence.loopMeta || !evidence.loopAllowsParallelFamily)
    return std::nullopt;
  if (evidence.totalMemrefs == 0)
    return std::nullopt;

  if (evidence.locallyVerifiedStencilMemrefs > 0 &&
      evidence.locallyVerifiedMultiDimStencilMemrefs > 0)
    return ArtsDepPattern::stencil;

  if (evidence.uniformMemrefs == evidence.totalMemrefs &&
      evidence.stencilMemrefs == 0)
    return ArtsDepPattern::uniform;

  return std::nullopt;
}

static std::optional<ArtsDepPattern>
chooseRefinedPattern(std::optional<ArtsDepPattern> existing,
                     std::optional<ArtsDepPattern> candidate,
                     const DiscoveryEvidence &evidence, DiscoveryMode mode) {
  if (!evidence.loopMeta || evidence.totalMemrefs == 0)
    return candidate;

  if (!existing)
    return candidate;

  if (mode == DiscoveryMode::Refine && *existing == ArtsDepPattern::stencil &&
      evidence.loopAllowsParallelFamily && evidence.multiDimStencilMemrefs > 0)
    return ArtsDepPattern::stencil_tiling_nd;

  if (candidate && *candidate == *existing)
    return existing;

  return std::nullopt;
}

static bool stampPatternContract(Operation *op, ArtsDepPattern pattern,
                                 int64_t revision) {
  bool changed = false;
  std::optional<ArtsDepPattern> existing = getDepPattern(op);
  if (!existing || *existing != pattern) {
    setDepPattern(op, pattern);
    changed = true;
  }

  int64_t currentRevision = getPatternRevision(op).value_or(0);
  if (currentRevision < revision) {
    setPatternRevision(op, revision);
    changed = true;
  }
  return changed;
}

static bool stampEdtContract(arts::EdtOp edt, ArtsDepPattern depPattern,
                             int64_t revision) {
  bool changed = false;
  if (!getDepPattern(edt.getOperation()) ||
      *getDepPattern(edt.getOperation()) != depPattern) {
    setDepPattern(edt.getOperation(), depPattern);
    changed = true;
  }
  if (auto distPattern = getDistributionPatternForDepPattern(depPattern)) {
    std::optional<EdtDistributionPattern> existing =
        getEdtDistributionPattern(edt.getOperation());
    if (!existing || *existing != *distPattern) {
      setEdtDistributionPattern(edt.getOperation(), *distPattern);
      changed = true;
    }
  }

  int64_t currentRevision = getPatternRevision(edt.getOperation()).value_or(0);
  if (currentRevision < revision) {
    setPatternRevision(edt.getOperation(), revision);
    changed = true;
  }
  return changed;
}

struct PatternDiscoveryPass
    : public impl::PatternDiscoveryBase<PatternDiscoveryPass> {
  PatternDiscoveryPass(mlir::arts::AnalysisManager *AM, bool refineMode)
      : analysisManager(AM) {
    assert(AM && "AnalysisManager must be provided externally");
    this->refine = refineMode;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    DiscoveryMode mode = refine ? DiscoveryMode::Refine : DiscoveryMode::Seed;
    int64_t targetRevision = revisionForMode(mode);
    auto &manager = analysisManager->getMetadataManager();

    int seeded = 0;
    int revised = 0;
    int skipped = 0;

    module.walk([&](arts::ForOp forOp) {
      LocalStencilEvidence localStencil =
          collectLocalStencilEvidence(forOp, targetRevision);
      DiscoveryEvidence evidence =
          collectEvidence(forOp, manager, localStencil);
      std::optional<StencilNDPatternContract> explicitStencil;
      if (mode == DiscoveryMode::Refine)
        explicitStencil = localStencil.explicitContract;
      std::optional<ArtsDepPattern> existing =
          getDepPattern(forOp.getOperation());
      std::optional<ArtsDepPattern> candidate =
          chooseMetadataCandidate(evidence);
      std::optional<ArtsDepPattern> chosen =
          chooseRefinedPattern(existing, candidate, evidence, mode);

      if (mode == DiscoveryMode::Refine && explicitStencil) {
        chosen = explicitStencil->getFamily();
      }

      if (!chosen) {
        ++skipped;
        return;
      }

      auto stampExplicitContract =
          [](Operation *op, const StencilNDPatternContract &contract) {
            if (!op)
              return false;
            DictionaryAttr before = op->getAttrDictionary();
            contract.stamp(op);
            return before != op->getAttrDictionary();
          };

      bool changed = false;
      if (explicitStencil && isStencilFamilyDepPattern(*chosen)) {
        changed |=
            stampExplicitContract(forOp.getOperation(), *explicitStencil);
      } else {
        changed =
            stampPatternContract(forOp.getOperation(), *chosen, targetRevision);
      }
      if (auto parentEdt = forOp->getParentOfType<arts::EdtOp>()) {
        if (explicitStencil && isStencilFamilyDepPattern(*chosen)) {
          changed |=
              stampExplicitContract(parentEdt.getOperation(), *explicitStencil);
        } else {
          changed |= stampEdtContract(parentEdt, *chosen, targetRevision);
        }
      }

      if (!existing && changed) {
        ++seeded;
      } else if (changed) {
        ++revised;
      } else {
        ++skipped;
      }
    });

    ARTS_INFO("PatternDiscoveryPass("
              << (mode == DiscoveryMode::Refine ? "refine" : "seed")
              << "): seeded=" << seeded << ", revised=" << revised
              << ", skipped=" << skipped);
  }

private:
  mlir::arts::AnalysisManager *analysisManager = nullptr;
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createPatternDiscoveryPass(AnalysisManager *AM,
                                                 bool refine) {
  return std::make_unique<PatternDiscoveryPass>(AM, refine);
}

} // namespace arts
} // namespace mlir
