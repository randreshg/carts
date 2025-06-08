///==========================================================================
/// File: DbAliasAnalysis.h
///
/// This class performs alias analysis for datablocks.
///==========================================================================

#ifndef CARTS_ANALYSIS_DBALIAS_ANALYSIS_H
#define CARTS_ANALYSIS_DBALIAS_ANALYSIS_H

#include "arts/Analysis/Db/DbInfo.h"
#include "mlir/Analysis/AliasAnalysis.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Value.h"
#include "llvm/ADT/DenseMap.h"

namespace mlir {
namespace arts {

/// DbAliasAnalysis
class DbAliasAnalysis {
public:
  explicit DbAliasAnalysis(DbAnalysis *analysis);
  ~DbAliasAnalysis() = default;

  /// Main alias analysis methods
  bool mayAlias(DbInfo &A, DbInfo &B);
  bool mayAlias(SmallVector<MemoryEffects::EffectInstance, 4> &effectsA,
                SmallVector<MemoryEffects::EffectInstance, 4> &effectsB);

private:
  DbAnalysis *analysis;
  DenseMap<std::pair<Value, Value>, bool> aliasCache;

  /// Enhanced memory region overlap analysis
  bool analyzeMemoryRegionOverlap(DbInfo &A, DbInfo &B);
  
  /// Dimension-wise overlap analysis
  bool analyzeDimensionOverlap(const DimensionAnalysis &dimA,
                              const DimensionAnalysis &dimB,
                              int64_t sizeA, int64_t sizeB,
                              size_t dimIndex);
  
  /// Pattern-specific overlap analysis methods
  bool analyzeConstantOverlap(const ComplexExpr &exprA,
                             const ComplexExpr &exprB);
  
  bool analyzeSequentialOverlap(const ComplexExpr &exprA,
                               const ComplexExpr &exprB,
                               int64_t sizeA, int64_t sizeB);
  
  bool analyzeStridedOverlap(const ComplexExpr &exprA,
                            const ComplexExpr &exprB,
                            int64_t sizeA, int64_t sizeB);
  
  bool analyzeBlockedOverlap(const ComplexExpr &exprA,
                            const ComplexExpr &exprB,
                            int64_t sizeA, int64_t sizeB);
  
  bool analyzeBoundsOverlap(const ComplexExpr &exprA,
                           const ComplexExpr &exprB);

  /// Utility for cache key generation
  std::pair<Value, Value> makeOrderedPair(Value A, Value B) {
    return A.getImpl() < B.getImpl() ? std::make_pair(A, B)
                                     : std::make_pair(B, A);
  }
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_DBALIAS_ANALYSIS_H