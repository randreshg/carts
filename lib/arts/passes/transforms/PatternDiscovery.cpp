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
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "arts/utils/metadata/MemrefMetadata.h"

#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Pass/Pass.h"
#include "llvm/ADT/SmallPtrSet.h"

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
};

static int64_t revisionForMode(DiscoveryMode mode) {
  return mode == DiscoveryMode::Refine ? 2 : 1;
}

static std::optional<EdtDistributionPattern>
distributionPatternFor(ArtsDepPattern pattern) {
  if (isStencilFamilyDepPattern(pattern))
    return EdtDistributionPattern::stencil;
  if (isUniformFamilyDepPattern(pattern))
    return EdtDistributionPattern::uniform;
  return std::nullopt;
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
  Value current = value;
  while (current) {
    Operation *def = current.getDefiningOp();
    if (!def)
      return nullptr;
    if (isa<memref::AllocOp, memref::AllocaOp>(def))
      return def;
    if (auto cast = dyn_cast<memref::CastOp>(def)) {
      current = cast.getSource();
      continue;
    }
    if (auto subview = dyn_cast<memref::SubViewOp>(def)) {
      current = subview.getSource();
      continue;
    }
    if (auto reinterpret = dyn_cast<memref::ReinterpretCastOp>(def)) {
      current = reinterpret.getSource();
      continue;
    }
    return nullptr;
  }
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

static DiscoveryEvidence collectEvidence(arts::ForOp forOp,
                                         MetadataManager &manager) {
  DiscoveryEvidence evidence;
  evidence.loopMeta = manager.getLoopMetadata(forOp.getOperation());
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
      if (isStencilMemref(*meta))
        ++evidence.stencilMemrefs;
      if (isMultiDimStencilMemref(*meta))
        ++evidence.multiDimStencilMemrefs;
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

  if (evidence.stencilMemrefs > 0 && evidence.multiDimStencilMemrefs > 0)
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
  if (auto distPattern = distributionPatternFor(depPattern)) {
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
    : public arts::PatternDiscoveryBase<PatternDiscoveryPass> {
  PatternDiscoveryPass() = default;
  PatternDiscoveryPass(mlir::arts::AnalysisManager *AM, bool refineMode)
      : analysisManager(AM) {
    this->refine = refineMode;
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    DiscoveryMode mode = refine ? DiscoveryMode::Refine : DiscoveryMode::Seed;
    int64_t targetRevision = revisionForMode(mode);

    std::unique_ptr<MetadataManager> localMetadata;
    MetadataManager *manager = nullptr;
    if (analysisManager) {
      manager = &analysisManager->getMetadataManager();
    } else {
      localMetadata = std::make_unique<MetadataManager>(&getContext());
      localMetadata->importFromOperations(module);
      manager = localMetadata.get();
    }

    int seeded = 0;
    int revised = 0;
    int skipped = 0;

    module.walk([&](arts::ForOp forOp) {
      DiscoveryEvidence evidence = collectEvidence(forOp, *manager);
      std::optional<ArtsDepPattern> existing =
          getDepPattern(forOp.getOperation());
      std::optional<ArtsDepPattern> candidate =
          chooseMetadataCandidate(evidence);
      std::optional<ArtsDepPattern> chosen =
          chooseRefinedPattern(existing, candidate, evidence, mode);

      if (!chosen) {
        ++skipped;
        return;
      }

      bool changed =
          stampPatternContract(forOp.getOperation(), *chosen, targetRevision);
      if (auto parentEdt = forOp->getParentOfType<arts::EdtOp>())
        changed |= stampEdtContract(parentEdt, *chosen, targetRevision);

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
