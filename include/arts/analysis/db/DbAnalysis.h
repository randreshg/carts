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
#include "arts/analysis/graphs/db/DbNode.h"
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
    /// Kept as a stored field because getAcquireContractSummary() applies
    /// post-hoc refinement (stencil/matmul fallback) that may differ from
    /// the raw facts value.
    AccessPattern accessPattern = AccessPattern::Unknown;
    /// Pointer to graph-owned facts. Valid for the lifetime of the DbGraph
    /// (i.e., within a single pass's runOnOperation()).
    const DbAcquirePartitionFacts *facts = nullptr;

    /// Delegating accessors — null-safe, return conservative defaults.
    bool hasIndirectAccess() const { return facts && facts->hasIndirectAccess; }
    bool hasDirectAccess() const { return facts && facts->hasDirectAccess; }
    bool hasBlockHints() const { return facts && facts->hasBlockHints; }
    bool inferredBlock() const { return facts && facts->inferredBlock; }
    bool hasFineGrainedEntries() const {
      return facts && facts->hasFineGrainedEntries();
    }
    bool hasUnmappedPartitionEntry() const {
      return facts && facts->hasUnmappedPartitionEntry();
    }
    bool preservesDistributedContractEntry() const {
      return facts && facts->hasDistributedContractEntries();
    }
    bool hasDistributionContract() const {
      return facts && facts->hasDistributionContract;
    }
    bool partitionDimsFromPeers() const {
      return facts && facts->partitionDimsFromPeers;
    }

    bool empty() const {
      return contract.empty() && accessPattern == AccessPattern::Unknown &&
             !facts;
    }
    bool usesStencilSemantics() const {
      return accessPattern == AccessPattern::Stencil ||
             contract.isStencilFamily() || contract.usesStencilDistribution() ||
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

  /// Return true when operations a and b have conflicting DB accesses:
  /// both access the same underlying DB allocation and at least one is a
  /// writer. This is a semantic query about DB relationships (conflict
  /// detection).
  static bool hasDbConflict(Operation *a, Operation *b);

  /// Query DB access patterns through the DB graph interface.
  std::optional<AccessPattern> getAcquireAccessPattern(DbAcquireOp acquire);
  std::optional<DbAccessPattern> getAllocAccessPattern(DbAllocOp alloc);

  /// Convenience: get DB alloc node by op (derives parent func internally).
  DbAllocNode *getDbAllocNode(DbAllocOp alloc);
  /// Convenience: get DB acquire node by op (derives parent func internally).
  DbAcquireNode *getDbAcquireNode(DbAcquireOp acquire);

  using ArtsAnalysis::getAnalysisManager;

  ///===----------------------------------------------------------------------===///
  /// Partition / Granularity Queries (static)
  ///===----------------------------------------------------------------------===///

  /// Get partition mode from DbAcquireOp structure.
  static PartitionMode getPartitionModeFromStructure(DbAcquireOp acquire);

  /// Get partition mode from DbAllocOp structure.
  static PartitionMode getPartitionModeFromStructure(DbAllocOp alloc);

  /// Check if acquire is in block partition mode.
  static bool isBlock(DbAcquireOp acquire);

  /// Check if acquire is in element-wise (fine_grained) partition mode.
  static bool isElementWise(DbAcquireOp acquire);

  /// Check if acquire is in coarse partition mode.
  static bool isCoarse(DbAcquireOp acquire);

  /// Check if allocation is coarse-grained (all sizes == 1).
  static bool isCoarseGrained(DbAllocOp alloc);

  /// Check if allocation is fine-grained (partitioned into multiple DBs).
  static bool isFineGrained(DbAllocOp alloc);

  /// Check if a datablock operation has a single size of 1.
  static bool hasSingleSize(Operation *dbOp);

  ///===----------------------------------------------------------------------===///
  /// Semantic Queries (static)
  ///===----------------------------------------------------------------------===///

  /// Conservative memory-object identity for memref-like values.
  static bool isSameMemoryObject(Value lhsMemref, Value rhsMemref);

  /// Check if a DbAcquireOp has static (constant) offset and size hints.
  static bool hasStaticHints(DbAcquireOp acqOp);

  /// Find the EDT operation that uses a DbControlOp result.
  static Operation *findUserEdt(DbControlOp dbControl);

  /// Check if a multi-entry acquire has a stencil access pattern.
  static bool hasMultiEntryStencilPattern(DbAcquireOp acquire,
                                          int64_t &minOffset,
                                          int64_t &maxOffset);

  /// Try to extract a constant offset between two index values.
  static std::optional<int64_t> getConstantOffsetBetween(Value idx, Value base);

  /// Returns true if the acquire belongs to an EDT with tiling_2d distribution.
  static bool isTiling2DTaskAcquire(DbAcquireOp acquire);

private:
  llvm::DenseMap<func::FuncOp, std::unique_ptr<DbGraph>> functionGraphMap;
  llvm::DenseMap<Operation *, LoopDbAccessSummary> loopAccessSummaryByOp;
  std::unique_ptr<DbAliasAnalysis> dbAliasAnalysis;
};

} // namespace arts
} // namespace mlir

#endif // ARTS_ANALYSIS_DB_DBANALYSIS_H
