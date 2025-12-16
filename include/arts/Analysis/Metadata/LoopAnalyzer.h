///===----------------------------------------------------------------------===///
// LoopAnalyzer.h - Loop Analysis for Metadata Collection
///===----------------------------------------------------------------------===///
//
// This file defines the LoopAnalyzer class, which performs comprehensive
// analysis of loop operations including parallelism detection, memory access
// patterns, and partitioning strategy recommendations.
//
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_LOOPANALYZER_H
#define ARTS_ANALYSIS_METADATA_LOOPANALYZER_H

#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Analysis/Metadata/DependenceAnalyzer.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/IR/Operation.h"
#include <optional>

namespace mlir {
namespace arts {

class ArtsMetadataManager; // Forward declaration

///===----------------------------------------------------------------------===///
// LoopAnalyzer - Loop analysis
///===----------------------------------------------------------------------===///
class LoopAnalyzer {
public:
  LoopAnalyzer(MLIRContext *context, AccessAnalyzer &accessAnalyzer,
               DependenceAnalyzer &depAnalyzer)
      : context(context), accessAnalyzer(accessAnalyzer),
        depAnalyzer(depAnalyzer) {}

  /// Populate metadata for loop operations
  void analyzeAffineLoop(affine::AffineForOp forOp, LoopMetadata *metadata);
  void analyzeSCFLoop(Operation *loopOp, LoopMetadata *metadata);

  /// Analyze loop nest for reordering opportunities.
  /// Only sets reorderNestTo if reordering is beneficial and legal.
  void analyzeLoopReordering(affine::AffineForOp outerLoop,
                             LoopMetadata *metadata,
                             ArtsMetadataManager &manager);

private:
  MLIRContext *context;
  AccessAnalyzer &accessAnalyzer;
  DependenceAnalyzer &depAnalyzer;

  void analyzeMemoryAccesses(Operation *loopOp, LoopMetadata *metadata);
  LoopMetadata::DataMovement classifyDataMovement(LoopMetadata *metadata);
  void suggestPartitioning(LoopMetadata *metadata);
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

#endif // ARTS_ANALYSIS_METADATA_LOOPANALYZER_H
