///===----------------------------------------------------------------------===///
// LoopAnalyzer.cpp - Loop Analysis Implementation
///===----------------------------------------------------------------------===///

#include "arts/Analysis/Metadata/LoopAnalyzer.h"
#include "arts/Analysis/Metadata/ArtsMetadataManager.h"
#include "arts/Utils/ArtsDebug.h"
#include "arts/Utils/ArtsUtils.h"
#include "arts/Utils/Metadata/LocationMetadata.h"
#include "mlir/Analysis/SliceAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/LoopAnalysis.h"
#include "mlir/Dialect/Affine/Analysis/Utils.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Affine/LoopUtils.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"

ARTS_DEBUG_SETUP(loop_analyzer);

using namespace mlir;
using namespace mlir::arts;

///===----------------------------------------------------------------------===///
// LoopAnalyzer Implementation
///===----------------------------------------------------------------------===///
void LoopAnalyzer::analyzeAffineLoop(affine::AffineForOp forOp,
                                     LoopMetadata *metadata) {
  /// Set location metadata
  metadata->locationMetadata = LocationMetadata::fromLocation(forOp.getLoc());

  /// Compute trip count from affine bounds
  if (forOp.hasConstantBounds()) {
    int64_t lb = forOp.getConstantLowerBound();
    int64_t ub = forOp.getConstantUpperBound();
    int64_t step = forOp.getStep();
    if (step > 0 && ub > lb)
      metadata->tripCount = (ub - lb + step - 1) / step;
  }

  /// Compute nesting level
  int64_t nestLevel = 0;
  Operation *parent = forOp->getParentOp();
  while (parent) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp>(
            parent)) {
      nestLevel++;
    }
    parent = parent->getParentOp();
  }
  metadata->nestingLevel = nestLevel;
  metadata->potentiallyParallel = isLoopParallel(forOp);

  metadata->hasInterIterationDeps = depAnalyzer.hasInterIterationDeps(forOp);
  detectReductions(forOp.getOperation(), metadata);
  finalizeParallelFlag(forOp.getOperation(), metadata);
}

void LoopAnalyzer::analyzeSCFLoop(Operation *loopOp, LoopMetadata *metadata) {
  /// Set location metadata
  metadata->locationMetadata = LocationMetadata::fromLocation(loopOp->getLoc());

  /// Compute nesting level
  int64_t nestLevel = 0;
  Operation *parent = loopOp->getParentOp();
  while (parent) {
    if (isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp>(
            parent)) {
      nestLevel++;
    }
    parent = parent->getParentOp();
  }
  metadata->nestingLevel = nestLevel;

  /// For scf.parallel and scf.forall, assume potentially parallel
  if (isa<scf::ParallelOp, scf::ForallOp>(loopOp)) {
    metadata->potentiallyParallel = true;
  } else if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    /// Try to compute trip count for scf.for
    if (auto lbConst =
            forOp.getLowerBound().getDefiningOp<arith::ConstantOp>()) {
      if (auto ubConst =
              forOp.getUpperBound().getDefiningOp<arith::ConstantOp>()) {
        if (auto stepConst =
                forOp.getStep().getDefiningOp<arith::ConstantOp>()) {
          auto lbAttr = lbConst.getValue().dyn_cast<IntegerAttr>();
          auto ubAttr = ubConst.getValue().dyn_cast<IntegerAttr>();
          auto stepAttr = stepConst.getValue().dyn_cast<IntegerAttr>();

          if (lbAttr && ubAttr && stepAttr) {
            int64_t lb = lbAttr.getInt();
            int64_t ub = ubAttr.getInt();
            int64_t step = stepAttr.getInt();
            if (step > 0 && ub > lb) {
              metadata->tripCount = (ub - lb + step - 1) / step;
            }
          }
        }
      }
    }
  } else if (auto whileOp = dyn_cast<scf::WhileOp>(loopOp)) {
    /// While loops have unknown trip count but carry block arguments similar to
    /// scf.for. Treat them conservatively (trip count unknown).
    (void)whileOp;
  }

  metadata->hasInterIterationDeps = depAnalyzer.hasInterIterationDeps(loopOp);
  detectReductions(loopOp, metadata);
  finalizeParallelFlag(loopOp, metadata);
}

void LoopAnalyzer::detectReductions(Operation *loopOp, LoopMetadata *metadata) {
  SmallVector<BlockArgument, 4> iterArgs;
  if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
    llvm::append_range(iterArgs, forOp.getRegionIterArgs());
  } else if (auto affineFor = dyn_cast<affine::AffineForOp>(loopOp)) {
    llvm::append_range(iterArgs, affineFor.getRegionIterArgs());
  } else {
    return;
  }

  if (iterArgs.empty())
    return;

  for (const auto &it : llvm::enumerate(iterArgs)) {
    SmallVector<Operation *, 4> combinerOps;
    Value reducedValue = matchReduction(
        iterArgs, static_cast<unsigned>(it.index()), combinerOps);
    if (!reducedValue)
      continue;
    metadata->hasReductions = true;
    LoopMetadata::ReductionKind kind = LoopMetadata::ReductionKind::Unknown;
    if (!combinerOps.empty()) {
      if (auto inferred = inferReductionKind(combinerOps.front()))
        kind = *inferred;
    }
    metadata->reductionKinds.push_back(kind);
  }
}

std::optional<LoopMetadata::ReductionKind>
LoopAnalyzer::inferReductionKind(Operation *op) const {
  if (!op)
    return std::nullopt;

  if (isa<arith::AddIOp, arith::AddFOp>(op))
    return LoopMetadata::ReductionKind::Add;
  if (isa<arith::MulIOp, arith::MulFOp>(op))
    return LoopMetadata::ReductionKind::Mul;
  if (isa<arith::MinSIOp, arith::MinUIOp, arith::MinimumFOp>(op))
    return LoopMetadata::ReductionKind::Min;
  if (isa<arith::MaxSIOp, arith::MaxUIOp, arith::MaximumFOp>(op))
    return LoopMetadata::ReductionKind::Max;
  if (isa<arith::AndIOp>(op))
    return LoopMetadata::ReductionKind::And;
  if (isa<arith::OrIOp>(op))
    return LoopMetadata::ReductionKind::Or;
  if (isa<arith::XOrIOp>(op))
    return LoopMetadata::ReductionKind::Xor;

  return std::nullopt;
}

void LoopAnalyzer::finalizeParallelFlag(Operation *loopOp,
                                        LoopMetadata *metadata) {
  bool hasDeps =
      metadata->hasInterIterationDeps && *metadata->hasInterIterationDeps;
  if (hasDeps) {
    metadata->potentiallyParallel = false;
    return;
  }

  if (!metadata->potentiallyParallel &&
      isa<affine::AffineForOp, scf::ForOp, scf::ParallelOp, scf::ForallOp,
          scf::WhileOp>(loopOp)) {
    metadata->potentiallyParallel = true;
  }
}

///===----------------------------------------------------------------------===///
// Per-Dimension Dependency Analysis
///===----------------------------------------------------------------------===///

void LoopAnalyzer::analyzeLoopNestDependences(affine::AffineForOp outerLoop,
                                              LoopMetadata *metadata) {
  // Use DependenceAnalyzer to get per-dimension dependency info
  auto result = depAnalyzer.analyzeLoopNestDependences(outerLoop);

  // Copy results to metadata
  metadata->dimensionDeps = std::move(result.dimensionDeps);
  metadata->outermostParallelDim = result.outermostParallelDim;

  // Debug output
  if (metadata->dimensionDeps.empty()) {
    ARTS_DEBUG("Per-dimension deps: single loop or no nest");
    return;
  }

  ARTS_DEBUG("Per-dimension deps analysis for loop nest:");
  for (const auto &dep : metadata->dimensionDeps) {
    ARTS_DEBUG(
        "  dim " << dep.dimension << ": "
                 << (dep.hasCarriedDep ? "carries deps" : "parallel")
                 << (dep.distance
                         ? " (dist=" + std::to_string(*dep.distance) + ")"
                         : ""));
  }

  if (metadata->outermostParallelDim) {
    ARTS_DEBUG(
        "Outermost parallel dimension: " << *metadata->outermostParallelDim);
  } else {
    ARTS_DEBUG("No parallelizable dimension found");
  }
}

///===----------------------------------------------------------------------===///
// Loop Reordering Analysis
///===----------------------------------------------------------------------===///

void LoopAnalyzer::analyzeLoopReordering(affine::AffineForOp outerLoop,
                                         LoopMetadata *metadata,
                                         ArtsMetadataManager &manager) {
  /// 1. Extract perfect nest
  SmallVector<affine::AffineForOp, 4> nest;
  affine::getPerfectlyNestedLoops(nest, outerLoop);

  if (nest.size() < 2) {
    ARTS_DEBUG("Loop reordering: nest size < 2, skipping");
    return; /// Nothing to reorder
  }

  /// 2. Check if perfectly nested (body has only inner loop + terminator)
  for (unsigned i = 0; i < nest.size() - 1; ++i) {
    auto &bodyOps = nest[i].getBody()->getOperations();
    /// Perfect nest means: inner loop + yield (terminator)
    if (bodyOps.size() != 2) {
      ARTS_DEBUG("Loop reordering: not perfectly nested at level " << i);
      return;
    }
  }

  /// 3. Compute optimal order
  SmallVector<unsigned> optimalPerm = computeOptimalOrder(nest);

  /// 4. Check if different from current order
  bool needsReorder = false;
  for (unsigned i = 0; i < nest.size(); ++i) {
    if (optimalPerm[i] != i) {
      needsReorder = true;
      break;
    }
  }
  if (!needsReorder) {
    ARTS_DEBUG("Loop reordering: already optimal");
    return; /// Already optimal - don't export anything
  }

  /// 5. Check if reordering is legal (dependence analysis)
  if (!affine::isValidLoopInterchangePermutation(nest, optimalPerm)) {
    ARTS_DEBUG("Loop reordering: permutation not valid (dependence conflict)");
    return; /// Can't reorder - don't export
  }

  /// 6. Build reorderNestTo using arts.ids
  metadata->reorderNestTo.clear();
  for (unsigned i = 0; i < optimalPerm.size(); ++i) {
    affine::AffineForOp targetLoop = nest[optimalPerm[i]];
    /// Get arts.id for this loop (ensure metadata exists)
    auto *loopMeta = manager.getLoopMetadata(targetLoop);
    if (!loopMeta) {
      ARTS_DEBUG("Loop reordering: missing metadata for loop in nest");
      metadata->reorderNestTo.clear();
      return;
    }
    auto artsId = loopMeta->getMetadataId();
    if (!artsId) {
      ARTS_DEBUG("Loop reordering: missing arts.id for loop in nest");
      metadata->reorderNestTo.clear();
      return;
    }
    metadata->reorderNestTo.push_back(*artsId);
  }

  ARTS_DEBUG("Loop reordering: set reorderNestTo with "
             << metadata->reorderNestTo.size() << " loops");
}

SmallVector<unsigned>
LoopAnalyzer::computeOptimalOrder(ArrayRef<affine::AffineForOp> nest) {
  /// Default: identity permutation
  SmallVector<unsigned> perm;
  for (unsigned i = 0; i < nest.size(); ++i)
    perm.push_back(i);

  if (nest.size() < 2)
    return perm;

  /// ARTS-aware optimization strategy:
  /// 1. Outer loop (index 0) often becomes parallel → PRESERVE POSITION
  /// 2. Inner loops should have stride-1 access for cache efficiency
  /// 3. For matmul A[i][k]*B[k][j]: make j innermost (B[k][j] stride-1)

  /// For 3-deep nests (common in matrix operations), try i-k-j order
  /// This is a heuristic for the common matmul pattern
  if (nest.size() == 3) {
    /// Check if we have matmul-like access pattern:
    /// A[i][k] * B[k][j] -> C[i][j]
    /// Original: i-j-k, Optimal: i-k-j (swap positions 1 and 2)

    /// For now, use simple heuristic: swap middle and inner loops
    /// This gives i-k-j from i-j-k, which is optimal for matmul
    perm = {0, 2, 1};
    ARTS_DEBUG("Loop reordering: trying i-k-j order for 3-deep nest");
  }

  /// TODO: Implement full stride analysis for general cases
  /// - Analyze memory access patterns in innermost loop
  /// - Compute stride for each memref along each loop dimension
  /// - Choose order that minimizes non-unit strides in inner loops

  return perm;
}
