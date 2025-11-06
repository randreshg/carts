//===----------------------------------------------------------------------===//
// LoopAnalyzer.h - Loop Analysis for Metadata Collection
//===----------------------------------------------------------------------===//
//
// This file defines the LoopAnalyzer class, which performs comprehensive
// analysis of loop operations including parallelism detection, memory access
// patterns, and partitioning strategy recommendations.
//
//===----------------------------------------------------------------------===//

#ifndef ARTS_ANALYSIS_METADATA_LOOPANALYZER_H
#define ARTS_ANALYSIS_METADATA_LOOPANALYZER_H

#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Analysis/Metadata/DependenceAnalyzer.h"
#include "arts/Utils/Metadata/LoopMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include <optional>

namespace mlir {
namespace arts {

//===----------------------------------------------------------------------===//
// LoopAnalyzer - Loop analysis
//===----------------------------------------------------------------------===//
class LoopAnalyzer {
public:
  LoopAnalyzer(MLIRContext *context, AccessAnalyzer &accessAnalyzer,
               DependenceAnalyzer &depAnalyzer)
      : context(context), accessAnalyzer(accessAnalyzer),
        depAnalyzer(depAnalyzer) {}

  /// Populate metadata for loop operations
  void analyzeAffineLoop(affine::AffineForOp forOp, LoopMetadata *metadata);
  void analyzeSCFLoop(Operation *loopOp, LoopMetadata *metadata);

private:
  MLIRContext *context;
  AccessAnalyzer &accessAnalyzer;
  DependenceAnalyzer &depAnalyzer;

  void analyzeMemoryAccesses(Operation *loopOp, LoopMetadata *metadata);
  LoopMetadata::DataMovement classifyDataMovement(LoopMetadata *metadata);
  void suggestPartitioning(LoopMetadata *metadata);
  void computeMemoryFootprintPerIter(Operation *loopOp, LoopMetadata *metadata);
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_LOOPANALYZER_H
