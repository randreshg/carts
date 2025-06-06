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

private:
  DbAnalysis *analysis;
  DenseMap<std::pair<Value, Value>, bool> aliasCache;

  /// Helper methods
  bool areFromSameAllocation(Value A, Value B);
  Operation *findRootAllocation(Value val);

  /// Utility for cache key generation
  std::pair<Value, Value> makeOrderedPair(Value A, Value B) {
    return A.getImpl() < B.getImpl() ? std::make_pair(A, B)
                                     : std::make_pair(B, A);
  }
};

} // namespace arts
} // namespace mlir

#endif // CARTS_ANALYSIS_DBALIAS_ANALYSIS_H