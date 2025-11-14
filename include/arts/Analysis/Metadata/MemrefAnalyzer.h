///===----------------------------------------------------------------------===///
// MemrefAnalyzer.h - Memory Reference Analysis for Metadata Collection
///===----------------------------------------------------------------------===///
//
// This file defines the MemrefAnalyzer class, which performs comprehensive
// analysis of memory allocations including access patterns, reuse distance,
// and lifetime analysis.
//
///===----------------------------------------------------------------------===///

#ifndef ARTS_ANALYSIS_METADATA_MEMREFANALYZER_H
#define ARTS_ANALYSIS_METADATA_MEMREFANALYZER_H

#include "arts/Analysis/Metadata/AccessAnalyzer.h"
#include "arts/Utils/ArtsId.h"
#include "arts/Utils/Metadata/MemrefMetadata.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <optional>

namespace mlir {
namespace arts {

class ArtsMetadataManager;

///===----------------------------------------------------------------------===///
// ReuseAnalyzer - Stack distance algorithm for reuse distance computation
///===----------------------------------------------------------------------===///
class ReuseAnalyzer {
public:
  explicit ReuseAnalyzer(AccessAnalyzer &accessAnalyzer)
      : accessAnalyzer(accessAnalyzer) {}

  std::optional<uint64_t> computeReuseDistance(Value memref,
                                               Operation *scopeOp);
  TemporalLocalityLevel classifyTemporalLocality(uint64_t reuseDistance);

private:
  AccessAnalyzer &accessAnalyzer;
  SmallVector<Operation *> collectAccessesInOrder(Value memref,
                                                  Operation *scopeOp);
  uint64_t computeStackDistance(Operation *access1, Operation *access2,
                                const SmallVector<Operation *> &allAccesses);
  uint64_t countUniqueMemrefsBetween(uint64_t startIdx, uint64_t endIdx,
                                     const SmallVector<Operation *> &accesses);
};

///===----------------------------------------------------------------------===///
// MemrefAnalyzer - Memory allocation analysis
///===----------------------------------------------------------------------===///
class MemrefAnalyzer {
public:
  MemrefAnalyzer(MLIRContext *context, AccessAnalyzer &accessAnalyzer,
                 ReuseAnalyzer &reuseAnalyzer,
                 ArtsMetadataManager &metadataManager)
      : context(context), accessAnalyzer(accessAnalyzer),
        reuseAnalyzer(reuseAnalyzer), metadataManager(metadataManager) {}

  void analyzeAllocation(Operation *allocOp, MemrefMetadata *metadata,
                         Operation *scopeOp);

private:
  MLIRContext *context;
  AccessAnalyzer &accessAnalyzer;
  ReuseAnalyzer &reuseAnalyzer;
  ArtsMetadataManager &metadataManager;

  std::pair<uint64_t, uint64_t> countAccesses(Value memref, Operation *scopeOp);

  std::pair<uint64_t, uint64_t> countAccessTypes(Value memref,
                                                 Operation *scopeOp);
  std::pair<ArtsId, ArtsId>
  computeLifetime(Value memref, Operation *scopeOp);
  void analyzeAccessCharacteristics(Value memref, MemrefMetadata *metadata,
                                    Operation *scopeOp);
  bool analyzeUniformAccess(Value memref, Operation *scopeOp);
  AccessPatternType classifyAccessPattern(Value memref,
                                          MemrefMetadata *metadata,
                                          Operation *scopeOp);
  bool isAccessedInsideParallelLoop(Value memref) const;
  bool hasLoopCarriedDependencies(Value memref) const;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_METADATA_MEMREFANALYZER_H
