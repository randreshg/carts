///==========================================================================///
/// File: DbAnalysis.h
///
/// This file defines the central analysis for DB operations.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "arts/Analysis/ArtsAnalysis.h"
#include "arts/Analysis/Graphs/Db/DbAccessPattern.h"
#include "arts/ArtsDialect.h"
#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include <optional>

namespace mlir {
namespace arts {

class DbGraph;
class DbAliasAnalysis;
class LoopAnalysis;

class DbAnalysis : public ArtsAnalysis {
public:
  struct AcquirePartitionSummary {
    PartitionMode mode = PartitionMode::coarse;
    SmallVector<Value> partitionOffsets;
    SmallVector<Value> partitionSizes;
    SmallVector<unsigned> partitionDims;
    bool isValid = false;
    bool hasIndirectAccess = false;
  };

  struct LoopDbAccessSummary {
    llvm::DenseMap<Operation *, DbAccessPattern> allocPatterns;
    bool hasStencilOffset = false;
    bool hasStencilAccessHint = false;
    bool hasMatmulUpdate = false;
    bool hasTriangularBound = false;
    EdtDistributionPattern distributionPattern =
        EdtDistributionPattern::unknown;
  };

  DbAnalysis(ArtsAnalysisManager &AM);

  ~DbAnalysis();

  /// Get or create DbGraph for a function, building if needed.
  DbGraph &getOrCreateGraph(func::FuncOp func);

  /// Invalidate graph for a function.
  bool invalidateGraph(func::FuncOp func);

  /// Invalidate all graphs
  void invalidate() override;

  /// Print analysis for a function.
  void print(func::FuncOp func);

  /// Access analyses
  DbAliasAnalysis *getAliasAnalysis() { return dbAliasAnalysis.get(); }
  LoopAnalysis *getLoopAnalysis();
  LoopDbAccessSummary analyzeLoopDbAccessPatterns(ForOp forOp);
  std::optional<LoopDbAccessSummary> getLoopDbAccessSummary(ForOp forOp);
  std::optional<EdtDistributionPattern> getLoopDistributionPattern(ForOp forOp);
  AcquirePartitionSummary analyzeAcquirePartition(DbAcquireOp acquire,
                                                  OpBuilder &builder);

  /// Return true when producerEdt writes DBs that are later consumed outside
  /// internode EDT flow.
  bool hasNonInternodeConsumerForWrittenDb(EdtOp producerEdt);

  /// Query DB access patterns through the DB graph interface.
  std::optional<AccessPattern> getAcquireAccessPattern(DbAcquireOp acquire);
  std::optional<DbAccessPattern> getAllocAccessPattern(DbAllocOp alloc);
  using ArtsAnalysis::getAnalysisManager;

private:
  llvm::DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  llvm::DenseMap<Operation *, LoopDbAccessSummary> loopAccessSummaryByOp;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H
