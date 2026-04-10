///==========================================================================///
/// File: PatternDiscovery.cpp
///
/// Local-analysis seed/refine pass for pre-DB pattern contracts.
///
/// The pass uses only local IR analysis to discover patterns:
/// - collectLocalStencilEvidence() walks the loop body for stencil detection
/// - collectEvidence() walks memref uses for affine/uniform classification
/// - arts::ForOp is inherently parallel (from omp.wsloop); reductions
///   are detected via the ForOp's reductionAccumulators operand
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/dialect/core/Transforms/kernel/KernelTransform.h"
#include "arts/dialect/core/Transforms/pattern/PatternTransform.h"
#include "arts/utils/ValueAnalysis.h"
#define GEN_PASS_DEF_PATTERNDISCOVERY
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
#include "arts/utils/DbUtils.h"
#include "arts/utils/LoopUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/Utils.h"
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
#include <memory>

#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(pattern_discovery);

using namespace mlir;
using namespace mlir::arts;

namespace {

enum class DiscoveryMode { Seed, Refine };

struct DiscoveryEvidence {
  bool loopAllowsParallelFamily = false;
  bool loopHasReductions = false;
  unsigned totalMemrefs = 0;
  unsigned affineMemrefs = 0;
  unsigned uniformMemrefs = 0;
  unsigned stencilMemrefs = 0;
  unsigned multiDimStencilMemrefs = 0;
};

static int64_t revisionForMode(DiscoveryMode mode) {
  return mode == DiscoveryMode::Refine ? 2 : 1;
}

static Operation *resolveAllocLikeSource(Value value) {
  Value stripped = ValueAnalysis::stripMemrefViewOps(value);
  Operation *def = stripped ? stripped.getDefiningOp() : nullptr;
  if (def && isa<memref::AllocOp, memref::AllocaOp>(def))
    return def;
  return nullptr;
}

/// Check if all uses of a memref within the loop body are via affine ops.
static bool isMemrefAllAffineInBody(Operation *allocLike, Block *body) {
  for (Operation &op : *body) {
    Value memref = DbUtils::getAccessedMemref(&op);
    if (!memref)
      continue;
    if (resolveAllocLikeSource(memref) != allocLike)
      continue;
    if (!isa<affine::AffineLoadOp, affine::AffineStoreOp>(&op))
      return false;
  }
  // Also check nested regions (inner loops, etc.)
  for (Operation &op : *body) {
    for (Region &region : op.getRegions()) {
      for (Block &block : region) {
        if (!isMemrefAllAffineInBody(allocLike, &block))
          return false;
      }
    }
  }
  return true;
}

/// Check if a memref has uniform (stride-one or all-affine) access in the body.
static bool isMemrefUniformInBody(Operation *allocLike, Block *body,
                                  ArrayRef<Value> ivs) {
  bool hasAccess = false;

  body->walk([&](Operation *op) {
    Value memref = DbUtils::getAccessedMemref(op);
    if (!memref)
      return;
    if (resolveAllocLikeSource(memref) != allocLike)
      return;

    hasAccess = true;

    // Affine ops are uniform by definition
    if (isa<affine::AffineLoadOp, affine::AffineStoreOp>(op))
      return;

    // For non-affine ops, check that the innermost index is stride-one (iv+c)
    SmallVector<Value> indices = DbUtils::getMemoryAccessIndices(op);
    if (indices.empty()) {
      hasAccess = false; // Can't verify — mark as non-uniform
      return;
    }

    Value innermost = indices.back();
    Value stripped = ValueAnalysis::stripNumericCasts(innermost);
    bool foundStrideOne = false;
    for (Value iv : ivs) {
      int64_t offset = 0;
      Value base = ValueAnalysis::stripNumericCasts(
          ValueAnalysis::stripConstantOffset(stripped, &offset));
      if (base == iv) {
        foundStrideOne = true;
        break;
      }
    }
    if (!foundStrideOne)
      hasAccess = false; // Non-stride-one access — not uniform
  });

  return hasAccess;
}

struct LocalStencilEvidence {
  bool isStencil = false;
  bool isMultiDimStencil = false;
  std::optional<StencilNDPatternContract> explicitContract;
};

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
    Value memref = DbUtils::getAccessedMemref(op);
    if (!memref)
      return;

    Operation *allocLike = resolveAllocLikeSource(memref);
    if (!allocLike)
      return;

    SmallVector<Value> indices = DbUtils::getMemoryAccessIndices(op);
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
collectEvidence(arts::ForOp forOp,
                const LocalStencilEvidence &localStencil) {
  DiscoveryEvidence evidence;

  // arts::ForOp is inherently parallel (created from omp.wsloop).
  // It only fails the parallel check if it has reductions.
  bool hasReductions = !forOp.getReductionAccumulators().empty();
  evidence.loopHasReductions = hasReductions;
  evidence.loopAllowsParallelFamily = !hasReductions;

  Block *body = forOp.getBody();
  if (!body)
    return evidence;

  // Collect induction variables for uniform access detection.
  SmallVector<Value, 4> ivs;
  Block *spatialBody = nullptr;
  if (!collectSpatialNestIvs(forOp, ivs, spatialBody)) {
    ivs.append(body->getArguments().begin(), body->getArguments().end());
  }

  llvm::SmallPtrSet<Operation *, 8> seenAllocs;
  body->walk([&](Operation *op) {
    Value memref = DbUtils::getAccessedMemref(op);
    if (!memref)
      return;

    Operation *allocLike = resolveAllocLikeSource(memref);
    if (!allocLike || !seenAllocs.insert(allocLike).second)
      return;

    ++evidence.totalMemrefs;

    if (isMemrefAllAffineInBody(allocLike, body))
      ++evidence.affineMemrefs;
    if (isMemrefUniformInBody(allocLike, body, ivs))
      ++evidence.uniformMemrefs;
    if (localStencil.isStencil)
      ++evidence.stencilMemrefs;
    if (localStencil.isMultiDimStencil)
      ++evidence.multiDimStencilMemrefs;
  });

  return evidence;
}

static std::optional<ArtsDepPattern>
chooseCandidate(const DiscoveryEvidence &evidence) {
  if (!evidence.loopAllowsParallelFamily)
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
  if (evidence.totalMemrefs == 0)
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

/// Helper to check if stamping a contract would change the operation's attrs.
static bool wouldContractChange(Operation *op,
                                const PatternContract &contract) {
  if (!op)
    return false;

  std::optional<ArtsDepPattern> existing = getDepPattern(op);
  if (!existing || *existing != contract.getFamily())
    return true;

  int64_t currentRevision = getPatternRevision(op).value_or(0);
  if (currentRevision < contract.getRevision())
    return true;

  // Check distribution pattern for all non-stencil contracts
  if (auto distPattern =
          getDistributionPatternForDepPattern(contract.getFamily())) {
    std::optional<EdtDistributionPattern> existingDist =
        getEdtDistributionPattern(op);
    if (!existingDist || *existingDist != *distPattern)
      return true;
  }

  return false;
}

struct PatternDiscoveryPass
    : public impl::PatternDiscoveryBase<PatternDiscoveryPass> {
  PatternDiscoveryPass(bool refineMode) { this->refine = refineMode; }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    DiscoveryMode mode = refine ? DiscoveryMode::Refine : DiscoveryMode::Seed;
    int64_t targetRevision = revisionForMode(mode);

    int seeded = 0;
    int revised = 0;
    int skipped = 0;

    module.walk([&](arts::ForOp forOp) {
      LocalStencilEvidence localStencil =
          collectLocalStencilEvidence(forOp, targetRevision);
      DiscoveryEvidence evidence = collectEvidence(forOp, localStencil);
      std::optional<StencilNDPatternContract> explicitStencil;
      if (mode == DiscoveryMode::Refine)
        explicitStencil = localStencil.explicitContract;
      std::optional<ArtsDepPattern> existing =
          getDepPattern(forOp.getOperation());
      std::optional<ArtsDepPattern> candidate = chooseCandidate(evidence);
      std::optional<ArtsDepPattern> chosen =
          chooseRefinedPattern(existing, candidate, evidence, mode);

      if (mode == DiscoveryMode::Refine && explicitStencil) {
        if (existing &&
            *existing == ArtsDepPattern::jacobi_alternating_buffers) {
          chosen = *existing;
        } else {
          chosen = explicitStencil->getFamily();
        }
      }

      if (!chosen) {
        ++skipped;
        return;
      }

      // Create the appropriate contract for this pattern
      std::unique_ptr<PatternContract> contract;
      if (explicitStencil && isStencilFamilyDepPattern(*chosen)) {
        contract = std::make_unique<StencilNDPatternContract>(
            *chosen, explicitStencil->getOwnerDims(),
            explicitStencil->getMinOffsets(), explicitStencil->getMaxOffsets(),
            explicitStencil->getWriteFootprint(),
            explicitStencil->getRevision(), explicitStencil->getBlockShape(),
            explicitStencil->getSpatialDims());
      } else {
        contract =
            std::make_unique<SimplePatternContract>(*chosen, targetRevision);
      }

      // Stamp the contract on the ForOp and its parent EdtOp
      bool changed = wouldContractChange(forOp.getOperation(), *contract);
      contract->stamp(forOp.getOperation());

      if (auto parentEdt = forOp->getParentOfType<arts::EdtOp>()) {
        changed |= wouldContractChange(parentEdt.getOperation(), *contract);
        contract->stamp(parentEdt.getOperation());
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
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createPatternDiscoveryPass(AnalysisManager *AM,
                                                 bool refine) {
  (void)AM; // No longer needed — kept for API compatibility
  return std::make_unique<PatternDiscoveryPass>(refine);
}

} // namespace arts
} // namespace mlir
