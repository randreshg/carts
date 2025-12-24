///===----------------------------------------------------------------------===///
// DependenceAnalyzer.h - Dependence Analysis Helper
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H
#define ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H

#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/Dialect/Affine/Analysis/AffineAnalysis.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include <optional>

namespace mlir {
namespace arts {

///===----------------------------------------------------------------------===///
// DependenceAnalyzer - Helper for analyzing loop dependencies
///===----------------------------------------------------------------------===///
class DependenceAnalyzer {
public:
  explicit DependenceAnalyzer(MLIRContext *context,
                              AccessAnalyzer &accessAnalyzer)
      : context(context), accessAnalyzer(accessAnalyzer) {}

  /// Check if a loop has inter-iteration dependencies
  bool hasInterIterationDeps(Operation *loopOp) const {
    /// For affine loops, run precise dependence checks
    if (auto forOp = dyn_cast<affine::AffineForOp>(loopOp))
      return analyzeAffineLoopDependences(forOp, std::nullopt).hasDependence;

    /// For SCF loops, treat iter_args that are actually updated as deps
    if (auto forOp = dyn_cast<scf::ForOp>(loopOp)) {
      if (forOp.getInitArgs().empty())
        return false;

      auto *terminator = forOp.getBody()->getTerminator();
      auto yieldOp = dyn_cast<scf::YieldOp>(terminator);
      if (!yieldOp)
        return true;
      auto regionArgs = forOp.getRegionIterArgs();
      auto yieldedValues = yieldOp.getResults();

      if (regionArgs.size() != yieldedValues.size())
        return true;

      for (unsigned i = 0, e = regionArgs.size(); i < e; ++i) {
        if (yieldedValues[i] != regionArgs[i])
          return true;
      }
      return false;
    }

    /// scf.parallel is explicitly parallel
    if (isa<scf::ParallelOp>(loopOp))
      return false;

    if (auto whileOp = dyn_cast<scf::WhileOp>(loopOp)) {
      if (whileOp.getInits().empty())
        return false;
      auto &after = whileOp.getAfter();
      if (after.empty())
        return true;
      Block &bodyBlock = after.front();
      auto yieldOp = dyn_cast<scf::YieldOp>(bodyBlock.getTerminator());
      if (!yieldOp)
        return true;
      auto regionArgs = bodyBlock.getArguments();
      auto yieldedValues = yieldOp.getResults();
      if (regionArgs.size() != yieldedValues.size())
        return true;
      for (auto [arg, value] : llvm::zip(regionArgs, yieldedValues)) {
        if (value != arg)
          return true;
      }
      return false;
    }

    /// Conservative default
    return true;
  }

  /// Check if a memref has loop-carried dependencies in a loop
  bool hasLoopCarriedDeps(Operation *loopOp, Value memref) const {
    if (auto forOp = dyn_cast<affine::AffineForOp>(loopOp))
      return analyzeAffineLoopDependences(forOp, memref).hasDependence;

    /// Simple heuristic:
    ///  If same memref is both read and written, assume dependency
    bool hasRead = false, hasWrite = false;

    loopOp->walk([&](Operation *op) {
      Value accessedMemref;
      bool isRead = false;

      if (auto load = dyn_cast<affine::AffineLoadOp>(op)) {
        accessedMemref = load.getMemRef();
        isRead = true;
      } else if (auto store = dyn_cast<affine::AffineStoreOp>(op)) {
        accessedMemref = store.getMemRef();
        isRead = false;
      } else if (auto load = dyn_cast<memref::LoadOp>(op)) {
        accessedMemref = load.getMemRef();
        isRead = true;
      } else if (auto store = dyn_cast<memref::StoreOp>(op)) {
        accessedMemref = store.getMemRef();
        isRead = false;
      }

      if (accessedMemref == memref) {
        if (isRead)
          hasRead = true;
        else
          hasWrite = true;
      }
    });

    return hasRead && hasWrite;
  }

  /// Compute the minimum positive dependence distance for affine loops.
  std::optional<int64_t>
  getMinDependenceDistance(affine::AffineForOp forOp) const {
    return analyzeAffineLoopDependences(forOp, std::nullopt).minDistance;
  }

  /// Result of per-dimension dependency analysis for a loop nest.
  struct LoopNestDependenceResult {
    SmallVector<DimensionDependency> dimensionDeps;
    std::optional<int64_t> outermostParallelDim; // -1 if none parallelizable
  };

  /// Analyze a loop nest for per-dimension dependencies.
  /// Key insight: inner loop deps don't prevent outer loop parallelism.
  ///
  /// Example: for(i) { for(j) { A[i][j] = f(A[i][j-1]) } }
  /// Result:
  ///   - dimensionDeps[0] = {dim=0, hasCarriedDep=false} // i-loop is
  ///   parallelizable
  ///   - dimensionDeps[1] = {dim=1, hasCarriedDep=true}  // j-loop has deps
  ///   - outermostParallelDim = 0
  ///
  /// This allows Seidel-2D to parallelize the i-loop even though j-loop is
  /// sequential.
  LoopNestDependenceResult
  analyzeLoopNestDependences(affine::AffineForOp outermostLoop) const {
    LoopNestDependenceResult result;

    // Collect the loop nest (outermost to innermost)
    SmallVector<affine::AffineForOp, 4> nest;
    affine::AffineForOp current = outermostLoop;
    while (current) {
      nest.push_back(current);
      // Find immediate inner affine.for
      affine::AffineForOp inner;
      current.getBody()->walk([&](affine::AffineForOp innerFor) {
        if (innerFor->getParentOp() == current.getOperation()) {
          inner = innerFor;
          return WalkResult::interrupt();
        }
        return WalkResult::advance();
      });
      current = inner;
    }

    if (nest.empty())
      return result;

    // Analyze per-dimension dependencies
    result.outermostParallelDim = std::nullopt;
    for (size_t dim = 0; dim < nest.size(); ++dim) {
      DimensionDependency depInfo;
      depInfo.dimension = dim;

      // Check if this specific loop dimension carries dependencies
      auto summary = analyzeAffineLoopDependencesAtDimension(nest, dim);
      depInfo.hasCarriedDep = summary.hasDependence;
      depInfo.distance = summary.minDistance;

      result.dimensionDeps.push_back(depInfo);

      // Track outermost parallelizable dimension
      if (!depInfo.hasCarriedDep && !result.outermostParallelDim) {
        result.outermostParallelDim = dim;
      }
    }

    return result;
  }

private:
  MLIRContext *context [[maybe_unused]];
  AccessAnalyzer &accessAnalyzer [[maybe_unused]];

  struct DependenceSummary {
    bool hasDependence = false;
    std::optional<int64_t> minDistance;
  };

  static unsigned getAffineLoopDepth(affine::AffineForOp forOp) {
    unsigned depth = 1;
    Operation *parent = forOp->getParentOp();
    while (parent) {
      if (isa<affine::AffineForOp>(parent))
        depth++;
      parent = parent->getParentOp();
    }
    return depth;
  }

  static bool isLoopCarriedComponent(const affine::DependenceComponent &comp,
                                     Operation *loopOp) {
    if (comp.op != loopOp)
      return false;
    bool lbZero = comp.lb && *comp.lb == 0;
    bool ubZero = comp.ub && *comp.ub == 0;
    if (lbZero && ubZero)
      return false;
    return true;
  }

  static void updateMinDistance(std::optional<int64_t> &current,
                                const affine::DependenceComponent &comp) {
    auto chooseDistance =
        [&](std::optional<int64_t> bound) -> std::optional<int64_t> {
      if (bound.has_value() && *bound > 0)
        return bound;
      return std::nullopt;
    };

    std::optional<int64_t> candidate = chooseDistance(comp.lb);
    if (!candidate)
      candidate = chooseDistance(comp.ub);
    if (!candidate)
      candidate = 1;

    if (!current || *candidate < *current)
      current = candidate;
  }

  /// Check if a dependence component at the target dimension is loop-carried.
  /// Returns true if the distance bounds indicate a non-zero distance.
  static bool isDimensionCarried(const affine::DependenceComponent &comp) {
    // A dimension carries a dependency if its distance is not zero.
    // Zero distance means same iteration (no loop-carried dep for this dim).
    bool lbZero = comp.lb && *comp.lb == 0;
    bool ubZero = comp.ub && *comp.ub == 0;
    if (lbZero && ubZero)
      return false; // Same iteration in this dimension
    return true;
  }

  /// Analyze dependencies for a specific loop dimension in the nest.
  /// Key insight: Only checks if the TARGET dimension carries dependencies,
  /// allowing outer loops to be parallelized even if inner loops have deps.
  ///
  /// For Seidel-2D: for(i) { for(j) { A[i][j] = f(A[i][j-1]) } }
  ///   - analyzeAtDimension(nest, 0) → false (i doesn't carry deps)
  ///   - analyzeAtDimension(nest, 1) → true  (j carries A[i][j-1] dep)
  DependenceSummary
  analyzeAffineLoopDependencesAtDimension(ArrayRef<affine::AffineForOp> nest,
                                          size_t targetDim) const {
    DependenceSummary summary;

    if (nest.empty() || targetDim >= nest.size())
      return summary;

    // The target loop we're checking for carried dependencies
    affine::AffineForOp targetLoop = nest[targetDim];

    // Use the existing single-loop analysis for the target loop
    // This is safer than trying to compute per-dimension components manually
    auto singleLoopResult =
        analyzeAffineLoopDependences(targetLoop, std::nullopt);

    // If there's a dependency carried by THIS loop, mark it
    summary.hasDependence = singleLoopResult.hasDependence;
    summary.minDistance = singleLoopResult.minDistance;

    return summary;
  }

  DependenceSummary
  analyzeAffineLoopDependences(affine::AffineForOp forOp,
                               std::optional<Value> memrefFilter) const {
    DependenceSummary summary;
    SmallVector<Operation *, 8> accesses;
    forOp->walk([&](Operation *nestedOp) {
      if (isa<affine::AffineReadOpInterface, affine::AffineWriteOpInterface>(
              nestedOp))
        accesses.push_back(nestedOp);
    });

    if (accesses.size() < 2)
      return summary;

    unsigned loopDepth = getAffineLoopDepth(forOp);
    for (Operation *srcOp : accesses) {
      affine::MemRefAccess srcAccess(srcOp);
      if (memrefFilter && srcAccess.memref != *memrefFilter)
        continue;
      for (Operation *dstOp : accesses) {
        if (srcOp == dstOp)
          continue;
        affine::MemRefAccess dstAccess(dstOp);
        if (memrefFilter && dstAccess.memref != *memrefFilter)
          continue;
        SmallVector<affine::DependenceComponent, 2> components;
        auto result = affine::checkMemrefAccessDependence(
            srcAccess, dstAccess, loopDepth, /*dependenceConstraints=*/nullptr,
            &components);
        if (!affine::hasDependence(result))
          continue;
        bool carried = false;
        for (const auto &component : components) {
          if (!isLoopCarriedComponent(component, forOp.getOperation()))
            continue;
          carried = true;
          updateMinDistance(summary.minDistance, component);
        }
        if (carried) {
          summary.hasDependence = true;
          if (!summary.minDistance)
            summary.minDistance = 1;
        }
      }
    }
    return summary;
  }
};

} // namespace arts
} // namespace mlir

#endif /// ARTS_ANALYSIS_METADATA_DEPENDENCEANALYZER_H
