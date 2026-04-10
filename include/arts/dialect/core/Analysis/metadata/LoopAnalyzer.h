///==========================================================================///
/// File: LoopAnalyzer.h
///
/// Loop Analysis for Metadata Collection
///==========================================================================///

#ifndef ARTS_DIALECT_CORE_ANALYSIS_METADATA_LOOPANALYZER_H
#define ARTS_DIALECT_CORE_ANALYSIS_METADATA_LOOPANALYZER_H

#include "arts/dialect/core/Analysis/metadata/DependenceAnalyzer.h"
#include "arts/utils/metadata/LoopMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Operation.h"
#include <optional>

namespace mlir {
namespace arts {

class MetadataManager;

///===----------------------------------------------------------------------===///
/// LoopAnalyzer - Loop analysis
///===----------------------------------------------------------------------===///
class LoopAnalyzer {
public:
  LoopAnalyzer(DependenceAnalyzer &depAnalyzer) : depAnalyzer(depAnalyzer) {}

  /// Populate metadata for loop operations
  void analyzeAffineLoop(affine::AffineForOp forOp, LoopMetadata *metadata);
  void analyzeSCFLoop(Operation *loopOp, LoopMetadata *metadata);

  /// Analyze loop nest for reordering opportunities.
  /// Only sets reorderNestTo if reordering is beneficial and legal.
  void analyzeLoopReordering(affine::AffineForOp outerLoop,
                             LoopMetadata *metadata, MetadataManager &manager);

  /// Analyze loop nest for per-dimension dependencies.
  /// Populates dimensionDeps and outermostParallelDim in metadata.
  /// Key insight: inner loop deps don't prevent outer loop parallelism.
  ///
  /// Example: for(i) { for(j) { A[i][j] = f(A[i][j-1]) } }
  ///   - dimensionDeps[0] = {dim=0, hasCarriedDep=false}
  ///   - dimensionDeps[1] = {dim=1, hasCarriedDep=true}
  ///   - outermostParallelDim = 0
  void analyzeLoopNestDependences(affine::AffineForOp outerLoop,
                                  LoopMetadata *metadata);

private:
  DependenceAnalyzer &depAnalyzer;

  void finalizeParallelFlag(Operation *loopOp, LoopMetadata *metadata);
  void detectReductions(Operation *loopOp, LoopMetadata *metadata);
  std::optional<LoopMetadata::ReductionKind>
  inferReductionKind(Operation *op) const;

  /// Compute optimal loop order for cache efficiency (ARTS-aware).
  /// Returns identity permutation if no beneficial reordering found.
  SmallVector<unsigned> computeOptimalOrder(ArrayRef<affine::AffineForOp> nest);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_DIALECT_CORE_ANALYSIS_METADATA_LOOPANALYZER_H
