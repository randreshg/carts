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

  /// Main alias analysis methods with optional indentation for debug output
  bool mayAlias(DbInfo &A, DbInfo &B, const std::string &indent = "");
  bool mayAlias(SmallVector<MemoryEffects::EffectInstance, 4> &effectsA,
                SmallVector<MemoryEffects::EffectInstance, 4> &effectsB,
                const std::string &indent = "");

private:
  DbAnalysis *analysis;
  DenseMap<std::pair<Value, Value>, bool> aliasCache;

  /// Enhanced memory region overlap analysis
  bool analyzeMemoryRegionOverlap(DbInfo &A, DbInfo &B, const std::string &indent);
  
  /// Dimension-wise overlap analysis
  bool analyzeDimensionOverlap(const DimensionAnalysis &dimA,
                              const DimensionAnalysis &dimB,
                              int64_t sizeA, int64_t sizeB,
                              size_t dimIndex, const std::string &indent);
  
  /// Pattern-specific overlap analysis methods
  bool analyzeConstantOverlap(const ComplexExpr &exprA,
                             const ComplexExpr &exprB, const std::string &indent);
  
  bool analyzeSequentialOverlap(const ComplexExpr &exprA,
                               const ComplexExpr &exprB,
                               int64_t sizeA, int64_t sizeB, const std::string &indent);
  
  bool analyzeStridedOverlap(const ComplexExpr &exprA,
                            const ComplexExpr &exprB,
                            int64_t sizeA, int64_t sizeB, const std::string &indent);
  
  bool analyzeBlockedOverlap(const ComplexExpr &exprA,
                            const ComplexExpr &exprB,
                            int64_t sizeA, int64_t sizeB, const std::string &indent);
  
  bool analyzeBoundsOverlap(const ComplexExpr &exprA,
                           const ComplexExpr &exprB, const std::string &indent);

  /// Utility for cache key generation
  std::pair<Value, Value> makeOrderedPair(Value A, Value B) {
    return A.getImpl() < B.getImpl() ? std::make_pair(A, B)
                                     : std::make_pair(B, A);
  }
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_DBALIAS_ANALYSIS_H