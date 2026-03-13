///==========================================================================///
/// File: DbAnalysis.h
///
/// This file defines the public DB analysis facade.
///
/// Responsibility split:
///   - DbGraph / DbNode own canonical graph-backed facts.
///   - DbAnalysis exposes pass-facing queries and lightweight summaries.
///   - Controller passes should ask DbAnalysis first instead of rebuilding DB
///     reasoning directly from raw IR or raw node methods.
///==========================================================================///

#ifndef ARTS_ANALYSIS_DB_DBANALYSIS_H
#define ARTS_ANALYSIS_DB_DBANALYSIS_H

#include "arts/Dialect.h"
#include "arts/analysis/Analysis.h"
#include "arts/analysis/graphs/db/DbAccessPattern.h"
#include "arts/utils/LoweringContractUtils.h"
#include "mlir/Analysis/DataFlowFramework.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/DenseMap.h"
#include <optional>

namespace mlir {
namespace arts {

class DbGraph;
class DbAllocNode;
class DbAcquireNode;
class DbAliasAnalysis;
class LoopAnalysis;
struct DbAcquirePartitionFacts;

class DbAnalysis : public ArtsAnalysis {
public:
  /// Pass-facing summary used by DbPartitioning while assembling heuristic and
  /// planner inputs. This is intentionally derived from graph-backed facts
  /// rather than a second canonical storage class.
  struct AcquirePartitionSummary {
    PartitionMode mode = PartitionMode::coarse;
    SmallVector<Value> partitionOffsets;
    SmallVector<Value> partitionSizes;
    SmallVector<unsigned> partitionDims;
    bool isValid = false;
    bool hasIndirectAccess = false;
    bool hasDistributionContract = false;
    bool partitionDimsFromPeers = false;
  };

  /// DB-side refined contract view.
  /// This keeps the same contract vocabulary used by lowering, but lets
  /// DbAnalysis strengthen incomplete contracts with graph-backed facts after
  /// DB creation. Pre-DB pattern passes seed the contract; post-DB analysis
  /// may refine it, but should not invent a separate contract language.
  struct AcquireContractSummary {
    LoweringContractInfo contract;
    bool refinedByDbAnalysis = false;
    AccessPattern accessPattern = AccessPattern::Unknown;
    bool hasIndirectAccess = false;
    bool hasDirectAccess = false;
    bool hasBlockHints = false;
    bool inferredBlock = false;
    bool hasFineGrainedEntries = false;
    bool hasUnmappedPartitionEntry = false;
    bool preservesDistributedContractEntry = false;
    bool hasDistributionContract = false;
    bool partitionDimsFromPeers = false;

    bool empty() const {
      return contract.empty() && accessPattern == AccessPattern::Unknown &&
             !hasIndirectAccess && !hasDirectAccess && !hasBlockHints &&
             !inferredBlock && !hasFineGrainedEntries &&
             !hasUnmappedPartitionEntry &&
             !preservesDistributedContractEntry &&
             !hasDistributionContract && !partitionDimsFromPeers;
    }
    bool usesStencilSemantics() const {
      return accessPattern == AccessPattern::Stencil ||
             contract.isStencilFamily() ||
             contract.usesStencilDistribution() ||
             contract.supportsBlockHalo();
    }
    bool usesMatmulSemantics() const {
      return (contract.depPattern &&
              *contract.depPattern == ArtsDepPattern::matmul) ||
             accessPattern == AccessPattern::Uniform;
    }
  };

  /// Loop-facing summary for H2 distribution selection and related consumers.
  /// This is a query result, not canonical ownership of DB facts.
  struct LoopDbAccessSummary {
    llvm::DenseMap<Operation *, DbAccessPattern> allocPatterns;
    bool hasStencilOffset = false;
    bool hasStencilAccessHint = false;
    bool hasMatmulUpdate = false;
    bool hasTriangularBound = false;
    EdtDistributionPattern distributionPattern =
        EdtDistributionPattern::unknown;
  };

  DbAnalysis(AnalysisManager &AM);

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
  std::optional<AcquireContractSummary>
  getAcquireContractSummary(DbAcquireOp acquire);
  bool operationHasDistributedDbContract(Operation *op);
  bool operationHasPeerInferredPartitionDims(Operation *op);
  /// Raw partition facts remain available for legality/detail consumers inside
  /// DB analysis and controller plumbing. Semantic pattern/lowering decisions
  /// should prefer getAcquireContractSummary().
  const DbAcquirePartitionFacts *getAcquirePartitionFacts(DbAcquireOp acquire);
  bool hasCrossElementSelfReadInLoop(DbAcquireOp acquire, ForOp loopOp);

  /// Return true when producerEdt writes DBs that are later consumed outside
  /// internode EDT flow.
  bool hasNonInternodeConsumerForWrittenDb(EdtOp producerEdt);

  /// Query DB access patterns through the DB graph interface.
  std::optional<AccessPattern> getAcquireAccessPattern(DbAcquireOp acquire);
  std::optional<DbAccessPattern> getAllocAccessPattern(DbAllocOp alloc);

  /// Convenience: get DB alloc node by op (derives parent func internally).
  DbAllocNode *getDbAllocNode(DbAllocOp alloc);
  /// Convenience: get DB acquire node by op (derives parent func internally).
  DbAcquireNode *getDbAcquireNode(DbAcquireOp acquire);

  using ArtsAnalysis::getAnalysisManager;

private:
  llvm::DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  llvm::DenseMap<Operation *, LoopDbAccessSummary> loopAccessSummaryByOp;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H
