///==========================================================================///
/// File: DbAnalysis.h
///
/// This file defines the public DB analysis facade.
///
/// Responsibility split:
///   - `arts.lowering_contract` and typed DB IR attributes are the canonical
///     semantic contract.
///   - DbGraph / DbNode cache derived graph summaries and legality evidence
///     projected from that IR.
///   - DbAnalysis exposes pass-facing queries that reconcile the canonical IR
///     contract with those derived caches.
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
  /// planner inputs. This is a transient query result synthesized from the
  /// canonical IR contract plus graph-derived caches, not a second authority.
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

  /// Canonical pass-facing contract view for an acquire.
  /// This keeps the same contract vocabulary used by lowering, while letting
  /// DbAnalysis strengthen incomplete IR contracts with graph-derived evidence
  /// after DB creation. Pre-DB pattern passes seed the contract; post-DB
  /// analysis may refine it, but should not invent a separate contract
  /// language or ask downstream consumers to reason over graph-only facts.
  struct AcquireContractSummary {
    LoweringContractInfo contract;
    bool refinedByDbAnalysis = false;
    /// Kept as a stored field because getAcquireContractSummary() applies
    /// post-hoc refinement (stencil/matmul fallback) that may differ from
    /// the raw facts value.
    AccessPattern accessPattern = AccessPattern::Unknown;
    /// Pointer to the derived graph cache that supported refinement. Valid for
    /// the lifetime of the DbGraph (i.e., within a single pass's
    /// runOnOperation()). Consumers may use this for legality/detail queries,
    /// but the semantic lowering contract remains `contract`.
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
      return contract.pattern.distributionKind ||
             contract.pattern.distributionPattern ||
             contract.pattern.distributionVersion;
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
    bool hasExplicitStencilLayout() const {
      return contract.hasExplicitStencilContract();
    }
    PartitionMode preferredBlockPartitionMode() const {
      return usesStencilSemantics() ? PartitionMode::stencil
                                    : PartitionMode::block;
    }
    bool supportsContractDrivenBlockCapability() const {
      return hasBlockHints() || inferredBlock() || usesStencilSemantics();
    }
    bool prefersSemanticOwnerLayoutPreservation() const {
      return contract.prefersSemanticOwnerLayoutPreservation();
    }
    bool prefersNDBlock(unsigned requiredRank = 2) const {
      return contract.prefersNDBlock(requiredRank);
    }
    bool usesWavefrontStencilContract() const {
      return contract.isWavefrontStencilContract();
    }
    bool usesMatmulSemantics() const {
      return contract.getEffectiveKind() == ContractKind::Matmul ||
             accessPattern == AccessPattern::Uniform;
    }
  };

  /// Loop-facing summary for H2 distribution selection and related consumers.
  /// This is a query result, not canonical storage for DB semantics.
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
  std::optional<unsigned> inferLoopMappedDim(DbAcquireOp acquire, ForOp forOp);
  std::optional<unsigned> inferLoopMappedDim(Value dep, ForOp forOp);
  AccessPattern resolveCanonicalAcquireAccessPattern(
      DbAcquireOp acquire, const AcquireContractSummary *summary = nullptr,
      const DbAcquirePartitionFacts *facts = nullptr);
  ArtsDepPattern resolveCanonicalAcquireDepPattern(
      DbAcquireOp acquire, const AcquireContractSummary *summary = nullptr,
      const DbAcquirePartitionFacts *facts = nullptr);
  bool hasCanonicalAcquireStencilSemantics(
      DbAcquireOp acquire, const AcquireContractSummary *summary = nullptr,
      const DbAcquirePartitionFacts *facts = nullptr);
  ArtsMode inferEdtAccessMode(Operation *underlyingOp, EdtOp edt) const;
  static ArtsMode classifyMemrefUserAccessMode(Operation *op,
                                               Operation *underlyingOp);
  static bool opMatchesAccessMode(Operation *op, Operation *underlyingOp,
                                  ArtsMode requestedMode);
  static bool accessModeCanSeedNestedAcquire(ArtsMode availableMode,
                                             ArtsMode requestedMode);
  bool operationHasDistributedDbContract(Operation *op);
  bool operationHasPeerInferredPartitionDims(Operation *op);
  /// Raw partition facts remain available as a derived cache for
  /// legality/detail consumers inside DB analysis and controller plumbing.
  /// Semantic pattern and lowering decisions should prefer
  /// getAcquireContractSummary().
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
